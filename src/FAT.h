#pragma once
#include "util.h"
#include <string>

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
    char volume_descriptor[250];    //popis vygenerovan�ho FS
    int8 fat_type;                //typ FAT (FAT12, FAT16...) 2 na fat_type - 1 cluster�
    int8 fat_copies;              //po�et kopi� FAT tabulek
    int16 cluster_size;           //velikost clusteru
    int32 usable_cluster_count;   //max po�et cluster�, kter� lze pou��t pro data (-konstanty)
    char signature[9];              //login autora FS
};// 272B

  //pokud bude ve FAT FAT_DIRECTORY, budou na disku v dan�m clusteru ulo�eny struktury o velikosti sizeof(directory) = 24B
struct Directory
{
    char name[13];                  //jm�no souboru, nebo adres��e ve tvaru 8.3'/0' 12 + 1
    bool isFile;                    //identifikace zda je soubor (TRUE), nebo adres�� (FALSE)
    int32 size;                   //velikost polo�ky, u adres��e 0
    int32 start_cluster;          //po��te�n� cluster polo�ky
};// 24B


class FAT
{
public:
    FAT(std::string filename);
    ~FAT();

    void loadBootRecod();
    void loadFatTables();
    void loadFS();
    void loadDir(class Node* root);

    void addFile(std::string file, std::string fatDir);
    void createDir(std::string dir, std::string parentDir);
    void remove(std::string name, clusterTypes type);
    void printFileClusters(std::string fileName);

    void printFile(std::string fileName);
    void printFile(Node* file);
    Node* find(Node* curr, std::string fileName);

    void printFat();
    void print(Node* node, uint32 level);
    void updateFatTables();
    void clearCluster(int32 cluster);
    void updateCluster(Node* node);
    void removeFromFatTables(int32 cluster, uint8 tableIndex, clusterTypes last);
private:
    BootRecord br;
    int32** fatTables;
    FILE* file;
    uint32 maxDirs;
    uint32 dataStart;
    Node* root;
};