#include "FAT.h"
#include "fs.h"

#include <iostream>

FAT::FAT(std::string filename)
    : fatTables(nullptr)
    , root(nullptr)
{
    file = fopen(filename.c_str(), "r+b");
    if (!file)
        throw std::runtime_error("Cant open fat file!");

    loadBootRecod();
    loadFatTables();
    loadFS();
}

void FAT::loadBootRecod()
{
    size_t res = fread(&br, sizeof(BootRecord), 1, file);
    if (!res)
        throw std::runtime_error("Error while reading boot record!");
    maxDirs = br.cluster_size / sizeof(Directory);
}

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

    dataStart = ftell(file);
}

void FAT::loadFS()
{
    root = new Node("", 0, false, 0, nullptr);
    loadDir(root);
}

void FAT::loadDir(Node* parent)
{
    fseek(file, dataStart + parent->cluster * br.cluster_size, SEEK_SET);
    std::vector < Node*> dirs;
    for (uint32 i = 0; i < maxDirs; i++)
    {
        Directory dir;
        size_t res = fread(&dir, sizeof(Directory), 1, file);
        // Since directory is not aligned, we cant check res (on win)
        /*if (!res)
            throw std::runtime_error("Error while reading directory!");
        */

        if (dir.start_cluster != 0)
        {
            Node* child = new Node(dir.name, dir.start_cluster, dir.isFile, dir.size, parent);
            parent->childs.push_back(child);
            if (!dir.isFile)
                dirs.push_back(child);
        }
        else
            break;
    }
    for (auto node : dirs)
        loadDir(node);
}

FAT::~FAT()
{
    if (fatTables)
    {
        for (uint8 i = 0; i < br.fat_copies; i++)
            if (fatTables[i])
                delete[] fatTables[i];
        delete[] fatTables;
    }

    if (root)
        delete root;

    fclose(file);
}

void FAT::addFile(std::string filename, std::string fatDir)
{
    if (fatDir[0] == '/')
        fatDir = fatDir.substr(1);
    Node* node = find(root, fatDir);
    if (!node || node->isFile)
        throw std::runtime_error("Path not found");

    FILE* newFile= fopen(filename.c_str(), "rb");
    if (!newFile)
        throw std::runtime_error("Cant open new file!");

    fseek(newFile, 0L, SEEK_END);
    uint32 size = ftell(newFile);
    fseek(newFile, 0L, SEEK_SET);
    uint32 nrCluster = size / br.cluster_size + !!(size % br.cluster_size);
    std::vector<int32> clusters;
    findFreeClusters(clusters, nrCluster);
    if (clusters.size() != nrCluster)
    {
        fclose(newFile);
        throw std::runtime_error("Not enough disc space");
    }

    char* buffer = new char[br.cluster_size];     
    for (uint32 i = 0; i < clusters.size(); i++)
    {
        memset(buffer, 0, br.cluster_size);
        size_t res = fread(buffer, br.cluster_size, 1, newFile);
        std::cout << buffer << std::endl;
        fseek(file, dataStart + clusters[i]*(br.cluster_size), SEEK_SET);
        fwrite(buffer, br.cluster_size, 1, file);

        for (uint8 j = 0; j < br.fat_copies; j++)
            fatTables[j][clusters[i]] = (i == clusters.size() - 1 ? FAT_FILE_END: clusters[i+1]);
    }
    node->childs.push_back(new Node(filename, clusters.front(), true, size, node));
    updateFatTables();
    updateCluster(node);

    delete[] buffer;
    fclose(newFile);
}

void FAT::createDir(std::string dir, std::string parentDir)
{
    if (parentDir[0] == '/')
        parentDir = parentDir.substr(1);

    Node* node = parentDir.empty() ? root : find(root, parentDir);
    if (!node || node->isFile)
        std::cout << "Path not found" << std::endl;
    else
    {
        int32 cluster = findFreeCluster();
        node->childs.push_back(new Node(dir, cluster, false, 0, node));
        for (uint8 i = 0; i < br.fat_copies; i++)
            fatTables[i][cluster] = FAT_DIRECTORY;
        updateFatTables();
        updateCluster(node);
        std::cout << "OK" << std::endl;
    }
}

void FAT::updateFatTables()
{
    fseek(file, sizeof(BootRecord), SEEK_SET);
    for (uint8 i = 0; i < br.fat_copies; i++)
    {
        size_t res =fwrite(fatTables[i], sizeof(int32), br.usable_cluster_count, file);
        if (res != br.usable_cluster_count)
            throw std::runtime_error("Cant update fat tables!");
    }
}

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

void FAT::updateCluster(Node* node)
{
    clearCluster(node->cluster);
    fseek(file, dataStart + node->cluster*(br.cluster_size), SEEK_SET);
    for (auto n : node->childs)
    {
        Directory dir;
        dir.isFile = n->isFile;
        memset(dir.name, 0, 13);
        strcpy(dir.name, n->name.c_str());
        dir.size = n->size;
        dir.start_cluster = n->cluster;         
   
        size_t res = fwrite(&dir, sizeof(Directory), 1, file);
        if (!res)
            throw std::runtime_error("Cant update cluster!");
    }
}


void FAT::removeFromFatTables(int32 cluster, uint8 tableIndex, clusterTypes last)
{
    if (fatTables[tableIndex][cluster] != last)
        removeFromFatTables(fatTables[tableIndex][cluster], tableIndex, last);

    fatTables[tableIndex][cluster] = FAT_UNUSED;
}

int32 FAT::findFreeCluster()
{
    for (int32 i = 1; i < br.usable_cluster_count; i++)
        if (fatTables[0][i] == FAT_UNUSED)
            return i;
    return -1;
}

void FAT::findFreeClusters(std::vector<int32>& clusters, uint32 nrCluster)
{
    for (int32 i = 1; i < br.usable_cluster_count; i++)
    {
        if (fatTables[0][i] == FAT_UNUSED)
            clusters.push_back(i);
        if (clusters.size() == nrCluster)
            break;
    }
}

void FAT::remove(std::string name, clusterTypes type)
{
    if (name[0] == '/')
        name = name.substr(1);
    Node* node = find(root, name);
    if (!node || (type == FAT_DIRECTORY && node->isFile) || (type == FAT_FILE_END && !node->isFile))
        std::cout << "Path not found" << std::endl;
    else if (!node->childs.empty())
        std::cout << "Not empty" << std::endl;
    else
    {
        for (uint8 i = 0; i < br.fat_copies; i++)
            removeFromFatTables(node->cluster, i, type);
        clearCluster(node->cluster);
        updateFatTables();
        Node* parent = node->parent;
        if (parent)
        {
            auto itr = std::find(parent->childs.begin(), parent->childs.end(), node);
            if (itr != parent->childs.end())
                parent->childs.erase(itr);
        }
        updateCluster(parent);
        delete node;
        std::cout << "OK" << std::endl;
    }
}

void FAT::printFileClusters(std::string fileName)
{
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

void FAT::printFile(std::string fileName)
{
    if (fileName[0] == '/')
        fileName = fileName.substr(1);
    Node* file = find(root, fileName);
    if (!file || !file->isFile)
        std::cout << "Path not found" << std::endl;
    else
    {
        std::cout << file->name << " ";
        printFile(file);
        std::cout << std::endl;
    }
}

void FAT::printFile(Node* node)
{
    int32 cluster = node->cluster;
    char* buffer = new char[br.cluster_size];
    do
    {
        memset(buffer, 0, sizeof(buffer));
        fseek(file, dataStart + cluster*(br.cluster_size), SEEK_SET);
        size_t res = fread(buffer, sizeof(char), br.cluster_size, file);
        if (res != br.cluster_size)
        {
            delete[] buffer;
            throw std::runtime_error("Failed read of cluster!");
        }
        std::cout << buffer;
        cluster = fatTables[0][cluster];
    } while (cluster != FAT_FILE_END);
    delete[] buffer;
}

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

void FAT::printFat()
{
    if (root->childs.empty())
    {
        std::cout << "Empty" << std::endl;
        return;
    }
    print(root, 0);
}

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
