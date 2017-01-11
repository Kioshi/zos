#include "FAT.h"
#include "fs.h"

#include <iostream>

FAT::FAT(std::string filename)
    : fatTables(nullptr)
    , root(nullptr)
{
    file = fopen(filename.c_str(), "r+");
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

    dataStart = ftell(file)-2;
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
        if (!res)
            throw std::runtime_error("Error while reading directory!");

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

void FAT::addFile(std::string file, std::string fatDir)
{
    throw std::logic_error("Not implemented yet");
}

void FAT::createDir(std::string dir, std::string parentDir)
{
    throw std::logic_error("Not implemented yet");
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
    char buffer[1] = { 0 };
    fseek(file, dataStart + cluster*(br.cluster_size), SEEK_SET);
    size_t res = fwrite(buffer, sizeof(char), br.cluster_size, file);
    if (res != br.cluster_size)
        throw std::runtime_error("Cant clean cluster!");
}

void FAT::updateCluster(Node* node)
{
    Directory* dirs = new Directory[node->childs.size()];
    for (uint32 i = 0; i < node->childs.size(); i++)
    {
        dirs[i].isFile = node->childs[i]->isFile;
        strcpy(dirs[i].name, node->childs[i]->name.c_str());
        dirs[i].size = node->childs[i]->size;
        dirs[i].start_cluster = node->childs[i]->cluster;
    }
    clearCluster(node->cluster);
    fseek(file, dataStart + node->cluster*(br.cluster_size), SEEK_SET);
    size_t res = fwrite(dirs, sizeof(Directory), node->childs.size(), file);
    if (res != node->childs.size())
        throw std::runtime_error("Cant update cluster!");
}


void FAT::removeFromFatTables(int32 cluster, uint8 tableIndex, clusterTypes last)
{
    if (fatTables[tableIndex][cluster] != last)
        removeFromFatTables(fatTables[tableIndex][cluster], tableIndex, last);

    fatTables[tableIndex][cluster] = FAT_UNUSED;
}

void FAT::remove(std::string name, clusterTypes type)
{
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
    for (auto child : curr->childs)
    {
        if (child->name == fileName)
            return child;
        else if (!child->isFile && fileName.find(child->name+"/") == 0)
        {
            return find(child, fileName.substr(child->name.size()+1));
        }
    }

    return NULL;
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
