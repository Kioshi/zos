#pragma once
#include "util.h"
#include <string>
#include <mutex>
#include <atomic>
#include <deque>
#include <vector>
#include <condition_variable>

//pocitame s FAT32 MAX - tedy horni 4 hodnoty
enum clusterTypes :int32
{
    FAT_DIRECTORY = INT32_MAX - 4,
    FAT_BAD_CLUSTER,
    FAT_FILE_END,
    FAT_UNUSED,
};

struct BootRecord
{
    char volume_descriptor[250];    //popis vygenerovaného FS
    int8 fat_type;                //typ FAT (FAT12, FAT16...) 2 na fat_type - 1 clusterù
    int8 fat_copies;              //poèet kopií FAT tabulek
    int16 cluster_size;           //velikost clusteru
    int32 usable_cluster_count;   //max poèet clusterù, který lze použít pro data (-konstanty)
    char signature[9];              //login autora FS
};// 272B

//pokud bude ve FAT FAT_DIRECTORY, budou na disku v daném clusteru uloženy struktury o velikosti sizeof(directory) = 24B
struct Directory
{
    char name[13];                  //jméno souboru, nebo adresáøe ve tvaru 8.3'/0' 12 + 1
    bool isFile;                    //identifikace zda je soubor (TRUE), nebo adresáø (FALSE)
    int32 size;                   //velikost položky, u adresáøe 0
    int32 start_cluster;          //poèáteèní cluster položky
};// 22B


class FAT
{
public:
    FAT(std::string filename);
    ~FAT();
private:
    void loadBootRecod();
    void loadFatTables();
    void loadFS();
    void loadDir(class Node* root);

    void dirLoader();

    void _printFile(Node* file);
    Node* find(Node* curr, std::string fileName);

    void print(Node* node, uint32 level);
    void updateFatTables();
    void clearCluster(int32 cluster);
    void updateCluster(Node* node);
    void removeFromFatTables(int32 cluster, uint8 tableIndex, clusterTypes last);
    int32 findFreeCluster();
    void findFreeClusters(std::vector<int32>& clusters, int32 nrCluster);
    void secureLoadDirs(char*buffer, long offset);
    void relocateBadDirsClusters();
    void moveCluster(int32 oldCluster, int32 newCluster);
public:
    void addFile(std::string file, std::string fatDir);
    void createDir(std::string dir, std::string parentDir);
    void remove(std::string name, clusterTypes type);
    void printFileClusters(std::string fileName);
    void printFile(std::string fileName);
    void printFat();
    bool isClusterBad(char* buffer, int32 cluster);
    std::string absName(Node* node);
    static void extractFilename(std::string& str);
    void corruptCluster(int32 cluster);
    void printFirstFewFatRows();
public:
    static uint8 max_threads;
private:
    BootRecord br;
    int32** fatTables;
    FILE* file;
    uint32 maxDirs;
    uint32 dataStart;
    Node* root;

    std::mutex loadLock;
    std::mutex dirsLock;
    std::mutex condLock;
    std::mutex badClustersLock;
    std::condition_variable condition;
    std::deque<Node*> dirsToLoad;
    std::deque<Node*> badClusters;
    std::atomic<uint32> working;
    std::atomic<uint32> dirs;
};