#include "FAT.h"
#include "fs.h"

#include <iostream>
#include <chrono>
#include <cstring>
#include <thread>
#include <algorithm>
#include <random>

uint8 FAT::max_threads;
FAT::FAT(std::string filename)
    : fatTables(nullptr)
    , root(nullptr)
    , working(0)
    , dirs(0)
{
    file = fopen(filename.c_str(), "r+b");
    if (!file)
        throw std::runtime_error("Cant open fat file!");

    loadBootRecod();
    loadFatTables();
    // Load filesystem into tree structure
    loadFS();
}

// Load boot recort into structure
void FAT::loadBootRecod()
{
    size_t res = fread(&br, sizeof(BootRecord), 1, file);
    if (!res)
        throw std::runtime_error("Error while reading boot record!");

    // Calculate maximum number of dirs in cluster for later user
    maxDirs = br.cluster_size / sizeof(Directory);
    if (br.cluster_size % sizeof(Directory) < 8)
    {
        if (maxDirs <= 1)
            throw std::runtime_error("Not enough room for directories.");
        else
            maxDirs--;
    }
}

// Load fat tables into multi dimensional array
void FAT::loadFatTables()
{
    fatTables = new int32*[br.fat_copies];
    memset(fatTables, 0, br.fat_copies);
    
    for (uint8 i = 0; i < br.fat_copies; i++)
    {
        fatTables[i] = new int32[br.usable_cluster_count];
        size_t res = fread(fatTables[i], sizeof(int32)*br.usable_cluster_count, 1, file);
        if (!res)
            throw std::runtime_error("Error while reading fat tables!");
    }

    // Save current position (start of clusters) for future use
    dataStart = ftell(file);
}

// Load direstories and files into tree structure
void FAT::loadFS()
{
    root = new Node("", 0, false, 0, nullptr);
    dirsToLoad.push_back(root);
    dirs++;

    // Create worker vectors to parse fat into filesystem
    std::vector<std::thread*> threads;
    for (uint8 i = 0; i < std::max((uint8)1,max_threads); i++)
        threads.push_back(new std::thread(&FAT::dirLoader,this));

    // Wait for threads and delete them
    for (auto* thread : threads)
    {
        thread->join();
        delete thread;
    }
    // Relocate bad dirs
    relocateBadDirsClusters();
}

// Load directories into buffer from file offset, locking file so other threads doesnt seek somewhere else before fread is executed
void FAT::secureLoadDirs(char*buffer, long offset)
{
    Guard guard(loadLock);
    fseek(file, offset, SEEK_SET);
    fread(buffer, br.cluster_size, 1, file);
}

// Consumer method for threads
void FAT::dirLoader()
{
    do
    {
        Node* node = nullptr;
        {
            // Condition lock
            Guard g1(condLock);
            // If someone still working and have no work to be done wait for signal
            while (dirs <= 0 && working)
                condition.wait(g1);

            // Critic section lock
            Guard g2(dirsLock);
            // Retrieve node from workload if there is any
            if (dirs)
            {
                working++;
                dirs--;
                node = dirsToLoad.front();
                dirsToLoad.pop_front();
            }
            // End of block call Guards destructors and free locks
        }
        // If we got node do some work
        if (node)
            loadDir(node);
    }
    // Well if others working it would be nice if this thread joined too
    while (working || dirs);
    condition.notify_all();
}

// Load dir content into filesystem
void FAT::loadDir(Node* parent)
{
    char* buffer = new char[br.cluster_size];
    // Lock then seek and load directories
    secureLoadDirs(buffer, dataStart + parent->cluster * br.cluster_size);
    if (isClusterBad(buffer, parent->cluster))
    {
        Guard g(badClustersLock);
        badClusters.push_back(parent);
    }

    Directory* directories = new Directory[maxDirs];
    memset(directories, 0, sizeof(Directory)*maxDirs);
    memcpy(directories, buffer, sizeof(Directory)*maxDirs);

    for (uint32 i = 0; i < maxDirs; i++)
    {
        Directory& dir = directories[i];
        dir.name[12] = 0;
        // If start cluester is not root
        if (dir.start_cluster != 0)
        {
            Node* child = new Node(dir.name, dir.start_cluster, dir.isFile, dir.size, parent);
            parent->addChild(child);
            // New dir found, yay more work to do
            if (!dir.isFile)
            {
                {
                    // Critical section lock
                    Guard guard(dirsLock);
                    dirs++;
                    dirsToLoad.push_back(child);
                }
                // Wake one consumer
                condition.notify_one();

            }
        }
        // Otherwise assume everything is fine and we just got to end of dir list filled with zeros
        else
            break;
    }
    delete[] directories;
    delete[] buffer;

    // If noone is working that mean all work is done, its time wake everyone and have a party, lets hope everyone joins
    if (--working == 0)
        condition.notify_all();
}

FAT::~FAT()
{
    fclose(file);

    if (fatTables)
    {
        for (uint8 i = 0; i < br.fat_copies; i++)
            if (fatTables[i])
                delete[] fatTables[i];
        delete[] fatTables;
    }

    if (root)
        delete root;

}

// Add file into FAT if exist and path to dir exists
void FAT::addFile(std::string filename, std::string fatDir)
{
    // Remove first / since our root have empty name
    if (fatDir[0] == '/')
        fatDir = fatDir.substr(1);

    // Dont need to / on end of path
    if (fatDir[fatDir.length() - 1] == '/')
        fatDir = fatDir.substr(0, fatDir.length() - 1);
    // Try to find node according to specified path
    Node* node = find(root, fatDir);
    if (!node || node->isFile)
        throw std::runtime_error("Path not found");
    if (Node* existing = find(node, filename))
        throw std::runtime_error("File/Dir with same name already in path");

    // Open new file
    FILE* newFile= fopen(filename.c_str(), "rb");
    if (!newFile)
        throw std::runtime_error("Cant open new file!");

    // Calculate size of new file
    fseek(newFile, 0L, SEEK_END);
    uint32 size = ftell(newFile);
    // Seet back to befining
    fseek(newFile, 0L, SEEK_SET);
    // Calculate number of clusters we need for new file
    uint32 nrCluster = size / br.cluster_size + !!(size % br.cluster_size);

    std::vector<int32> clusters;
    // Find free clusters in fat
    findFreeClusters(clusters, nrCluster);
    // Check if there is enough space for file
    if (clusters.size() != nrCluster)
    {
        fclose(newFile);
        throw std::runtime_error("Not enough disc space");
    }

    // Copy files into clusters
    char* buffer = new char[br.cluster_size];     
    for (uint32 i = 0; i < clusters.size(); i++)
    {
        memset(buffer, 0, br.cluster_size);
        // Read cluster from new file
        size_t res = fread(buffer, br.cluster_size, 1, newFile);
        // Seek to cluster
        fseek(file, dataStart + clusters[i]*(br.cluster_size), SEEK_SET);
        // Write buffer to fat
        fwrite(buffer, br.cluster_size, 1, file);

        // Update FAT
        for (uint8 j = 0; j < br.fat_copies; j++)
            fatTables[j][clusters[i]] = (i == clusters.size() - 1 ? FAT_FILE_END: clusters[i+1]);
    }

    extractFilename(filename);
    // Push new file into filesystem
    node->childs.push_back(new Node(filename, clusters.front(), true, size, node));
    updateFatTables();
    updateCluster(node);

    delete[] buffer;
    fclose(newFile);
    std::cout << "OK" << std::endl;
}

// Create dir in parentDir
void FAT::createDir(std::string dir, std::string parentDir)
{
    // Remove first / since our root have empty name
    if (parentDir[0] == '/')
        parentDir = parentDir.substr(1);

    // Dont save / on end of the name
    if (dir[dir.length() - 1] == '/')
        dir = dir.substr(0, dir.length() - 1);
    // Dont need to / on end of path
    if (parentDir[parentDir.length() - 1] == '/')
        parentDir = parentDir.substr(0, parentDir.length() - 1);

    // Try to find node according to specified path
    Node* node = parentDir.empty() ? root : find(root, parentDir);
    if (!node || node->isFile)
        std::cout << "Path not found" << std::endl;
    else if (Node* existing = find(node, dir))
        std::cout << "File/Dir with same name already in path" << std::endl;
    else
    {
        // Find a free cluster
        int32 cluster = findFreeCluster();
        if (cluster == -1)
            throw std::runtime_error("Not enough disc space");
        // Add new dir into FS
        node->childs.push_back(new Node(dir, cluster, false, 0, node));
        // Update FAT
        for (uint8 i = 0; i < br.fat_copies; i++)
            fatTables[i][cluster] = FAT_DIRECTORY;
        updateFatTables();
        updateCluster(node);
        std::cout << "OK" << std::endl;
    }
}

// Update FAT tables into file
void FAT::updateFatTables()
{
    // Seek to fat section
    fseek(file, sizeof(BootRecord), SEEK_SET);
    //
    for (uint8 i = 0; i < br.fat_copies; i++)
    {
        size_t res =fwrite(fatTables[i], sizeof(int32), br.usable_cluster_count, file);
        if (res != br.usable_cluster_count)
            throw std::runtime_error("Cant update fat tables!");
    }
}

// File cluster with zeros
void FAT::clearCluster(int32 cluster)
{
    char* buffer = new char[br.cluster_size];
    memset(buffer, 0, br.cluster_size);
    fseek(file, dataStart + cluster*(br.cluster_size), SEEK_SET);
    size_t res = fwrite(buffer, br.cluster_size, 1, file);
    delete[] buffer;
    if (!res)
        throw std::runtime_error("Cant clean cluster!");
}

// Update cluster into file
void FAT::updateCluster(Node* node)
{
    // Clear cluster
    clearCluster(node->cluster);
    fseek(file, dataStart + node->cluster*(br.cluster_size), SEEK_SET);
    // Write directories
    for (auto n : node->childs)
    {
        Directory dir;
        dir.isFile = n->isFile;
        memset(dir.name, 0, 13);
        strncpy(dir.name, n->name.c_str(),12);
        dir.size = n->size;
        dir.start_cluster = n->cluster;         
   
        size_t res = fwrite(&dir, sizeof(Directory), 1, file);
        if (!res)
            throw std::runtime_error("Cant update cluster!");
    }
}

// Free all clusters from fat tables
void FAT::removeFromFatTables(int32 cluster, uint8 tableIndex, clusterTypes last)
{
    if (fatTables[tableIndex][cluster] != last)
        removeFromFatTables(fatTables[tableIndex][cluster], tableIndex, last);

    if (tableIndex == 0)
        clearCluster(cluster);

    fatTables[tableIndex][cluster] = FAT_UNUSED;
}

// Find free cluster, return -1 if there is not one
int32 FAT::findFreeCluster()
{
    for (int32 i = 1; i < br.usable_cluster_count; i++)
        if (fatTables[0][i] == FAT_UNUSED)
            return i;
    return -1;
}

// Find free number clusters 
void FAT::findFreeClusters(std::vector<int32>& clusters, int32 nrCluster)
{
    for (int32 i = 1; i < br.usable_cluster_count; i++)
    {
        if (fatTables[0][i] == FAT_UNUSED)
            clusters.push_back(i);
        if (clusters.size() == nrCluster)
            break;
    }
}

// Remove file or directory
void FAT::remove(std::string name, clusterTypes type)
{
    // Remove first / since our root have empty name
    if (name[0] == '/')
        name = name.substr(1);

    // Dont need to / on end of path
    if (type == FAT_DIRECTORY && name[name.length() - 1] == '/')
        name = name.substr(0, name.length() - 1);

    // Try to find file/dir to remove
    Node* node = find(root, name);
    // Validate
    if (!node || (type == FAT_DIRECTORY && node->isFile) || (type == FAT_FILE_END && !node->isFile))
        std::cout << "Path not found" << std::endl;
    else if (!node->childs.empty())
        std::cout << "Not empty" << std::endl;
    else
    {
        // Free all clusters owned by file/dir
        for (uint8 i = 0; i < br.fat_copies; i++)
            removeFromFatTables(node->cluster, i, type);

        // Sync fat tables into file
        updateFatTables();
        Node* parent = node->parent;
        if (parent)
        {
            // Remove file/dir from parent
            auto itr = std::find(parent->childs.begin(), parent->childs.end(), node);
            if (itr != parent->childs.end())
                parent->childs.erase(itr);
        }
        // Update parent
        updateCluster(parent);
        delete node;
        std::cout << "OK" << std::endl;
    }
}

// Print all clusters of file
void FAT::printFileClusters(std::string fileName)
{
    // Remove first / since our root have empty name
    if (fileName[0] == '/')
        fileName = fileName.substr(1);

    Node* node = find(root, fileName);
    if (!node || !node->isFile)
        std::cout << "Path not found" << std::endl;
    else
    {
        std::cout << node->name << " ";
        int32 cluster = node->cluster;
        do
        {
            std::cout << cluster << " ";
            cluster = fatTables[0][cluster];
        } while (cluster != FAT_FILE_END);
        std::cout << std::endl;
    }
}

// Print contents of file
void FAT::printFile(std::string fileName)
{
    // Try to find file/dir to remove
    if (fileName[0] == '/')
        fileName = fileName.substr(1);

    Node* file = find(root, fileName);
    if (!file || !file->isFile)
        std::cout << "Path not found" << std::endl;
    else
    {
        //std::cout << file->name << " ";
        _printFile(file);
        //std::cout << std::endl;
    }
}

// Print cluster content, check for bad cluster and try to fix it
void FAT::_printFile(Node* node)
{
    int32 cluster = node->cluster;
    int32 prevCluster = -1;
    char* buffer = new char[br.cluster_size+1];
    do
    {
        memset(buffer, 0, br.cluster_size + 1);
        fseek(file, dataStart + cluster*(br.cluster_size), SEEK_SET);
        size_t res = fread(buffer, sizeof(char), br.cluster_size, file);
        if (res != br.cluster_size)
        {
            delete[] buffer;
            throw std::runtime_error("Failed read of cluster!");
        }

        if (isClusterBad(buffer, cluster))
        {
            std::cout << std::endl << "Relocating bad cluster!" << std::endl;
            int32 newCluster = findFreeCluster();
            if (cluster == -1)
                throw std::runtime_error("Not enough room for realocate bad cluster!");
            if (prevCluster == -1)
            {
                node->cluster = newCluster;
                if (node->parent)
                    updateCluster(node->parent);
            }
            for (uint8 i = 0; i < br.fat_copies; i++)
            {
                fatTables[i][newCluster] = fatTables[i][cluster];
                if (prevCluster != -1)
                    fatTables[i][prevCluster] = newCluster;
            }
            moveCluster(cluster, newCluster);
            prevCluster = newCluster;
            fatTables[0][cluster] = FAT_BAD_CLUSTER;
            cluster = fatTables[0][newCluster];
            updateFatTables();
            std::cout << buffer;
            continue;
        }
        std::cout << buffer;
        prevCluster = cluster;
        cluster = fatTables[0][cluster];
    } while (cluster != FAT_FILE_END);
    delete[] buffer;
}

// Check if cluster is bad and try to fix it
bool FAT::isClusterBad(char* buffer, int32 cluster)
{
    // Compare first and last 8 bytes, if they dont match or doesnt contain letter F, cluster is fine
    for (uint8 i = 0; i < 8; i++)
        if (buffer[i] != buffer[br.cluster_size - 8 + i] || buffer[i] != 'F')
            return false;

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> distr(0, RANDOM_RANGE);

    {
        // Set to f so next time we dont detect it as bad sector next time
        memset(buffer, 'f', 8);
        memset(buffer + br.cluster_size - 8, 'f', 8);
        Guard guard(loadLock);
        fseek(file, dataStart + cluster*(br.cluster_size), SEEK_SET);
        fwrite(buffer, sizeof(char)* br.cluster_size, 1, file);
    }

    std::cout << "\nFirst and last 8 bytes lost!";
    // Rolling a dice, if result is zero we repaired it
    uint32 random = distr(eng);
    if (!random)
    {
        std::cout << "\nFixed bad cluster!\n";
        return false;
    }

    // Cluster is bad
    return true;
}

// Get name with absolute path of node
std::string FAT::absName(Node* node)
{
    if (!node->parent)
        return node->name;

    return absName(node->parent) + "/" + node->name;
}

// Find node in filesystem
Node* FAT::find(Node* curr, std::string fileName)
{
    if (fileName.empty())
        return curr;

    for (auto child : curr->childs)
    {
        if (child->name == fileName)
            return child;
        else if (!child->isFile && fileName.find(child->name+"/") == 0)
        {
            return find(child, fileName.substr(child->name.size()+1));
        }
    }
    return nullptr;
}

// 
void FAT::printFat()
{
    if (root->childs.empty())
    {
        std::cout << "Empty" << std::endl;
        return;
    }
    print(root, 0);
}

// Print filesystem
void FAT::print(Node* node, uint32 level)
{
    for (uint32 i = 0; i < level; i++)
        std::cout << "\t";
    std::cout << (node->isFile ? "-" : "+") << (node->name.empty() ? "ROOT" : node->name);

    if (!node->isFile)
    {
        std::cout << std::endl;
        for (auto child : node->childs)
            print(child, level + 1);

        for (uint32 i = 0; i < level; i++)
            std::cout << '\t';

        std::cout << "--" << std::endl;
    }
    else
    {
        std::cout << " " << node->cluster << " " << ((node->size / br.cluster_size) + !!(node->size % br.cluster_size)) << std::endl;
    }
}

void FAT::extractFilename(std::string& str)
{
    size_t found = str.find_last_of("/\\");
    str = str.substr(found + 1);
}

void FAT::relocateBadDirsClusters()
{
    if (!badClusters.empty())
        std::cout << std::endl;
    for (auto node : badClusters)
    {
        int32 cluster = findFreeCluster();
        if (cluster == -1)
            throw std::runtime_error("Not enough room for realocate bad cluster!");
        moveCluster(node->cluster, cluster);
        for (int8 i = 0; i < br.fat_copies; i++)
        {
            fatTables[i][cluster] = FAT_DIRECTORY;
            fatTables[i][node->cluster] = FAT_BAD_CLUSTER;
        }
        updateFatTables();
        std::cout << "Moving bad dir cluster from " << (int)node->cluster << " to " << (int)cluster << std::endl;
        node->cluster = cluster;
        if (node->parent)
            updateCluster(node->parent);
        else
            throw std::runtime_error("Corrupted FAT!");
    }
    badClusters.clear();
}

void FAT::moveCluster(int32 oldCluster, int32 newCluster)
{
    char* buffer = new char[br.cluster_size];
    fseek(file, dataStart + oldCluster*(br.cluster_size), SEEK_SET);
    fread(buffer, br.cluster_size, 1, file);
    fseek(file, dataStart + newCluster*(br.cluster_size), SEEK_SET);
    fwrite(buffer, br.cluster_size, 1, file);
    delete[] buffer;
    clearCluster(oldCluster);
}

void FAT::corruptCluster(int32 cluster)
{
    char* buffer = new char[br.cluster_size];
    fseek(file, dataStart + cluster*(br.cluster_size), SEEK_SET);
    fread(buffer, sizeof(char)* br.cluster_size, 1, file);
    memset(buffer, 'F', 8);
    memset(buffer + br.cluster_size - 8, 'F', 8);
    fseek(file, dataStart + cluster*(br.cluster_size), SEEK_SET);
    fwrite(buffer, sizeof(char)* br.cluster_size, 1, file);
}

void FAT::printFirstFewFatRows()
{
    for (int i = 0; i < 20; i++)
    {
        std::cout << i << ": ";
        switch (fatTables[0][i])
        {
            case FAT_BAD_CLUSTER:
                std::cout << "Bad cluster";
                break;
            case FAT_DIRECTORY:
                std::cout << "Directory";
                break;
            case FAT_FILE_END:
                std::cout << "File end";
                break;
            case FAT_UNUSED:
                std::cout << "Unused";
                break;
            default:
                std::cout << (int)fatTables[0][i];
                break;
        }
        std::cout << std::endl;
    }
}
