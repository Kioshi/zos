#include <iostream>
#include "FAT.h"
#include <cstring>
#include <algorithm>
#include <random>
#include "util.h"

void create(int16 cluster_count, int16 cluster_size)
{
    FILE *file = fopen("empty.fat", "wb");

    // boot record
    BootRecord br;
    memset(&br, 0, sizeof(BootRecord));
    strcpy(br.volume_descriptor, "big empty fat");
    strcpy(br.signature, "smartine");
    br.cluster_size = cluster_size;
    br.usable_cluster_count = cluster_count;
    br.fat_type = 8;
    br.fat_copies = 2;

    char* cluster = new char[cluster_size];
    memset(cluster, 0, cluster_size);

    int32* fat = new int32[cluster_count];
    for (int32 i = 0; i < cluster_count; i++)
        fat[i] = FAT_UNUSED;

    fat[0] = FAT_DIRECTORY;

    fwrite(&br, sizeof(BootRecord), 1, file);
    for (uint8 i = 0; i < br.fat_copies; i++)
        fwrite(fat, sizeof(int32)*cluster_count, 1, file);

    for (int32 j = 0; j < br.usable_cluster_count; j++)
        fwrite(cluster, cluster_size, 1, file);

    fclose(file);
    delete[] cluster;
    delete[] fat;
}

bool validateArguments(int argc, char *argv[])
{
    // Arguments must compose from <path to fat file> <command>
    if (argc < 3)
    {
        std::cout << "Not enough arguments." << std::endl;
        std::cout << "Syntax <path to fat> <command>" << std::endl;
        return false;
    }

    if (strcmp("-g", argv[1]) == 0)
    {
        if (argc == 4)
            create(atoi(argv[2]), atoi(argv[3]));
        else
            std::cout << "Syntax for -g is <cluster count> <cluster size>" << std::endl;
        return false;
    }
    // Check if command is made from 2 and more characters since we select command from second one
    if (strlen(argv[2]) < 2)
    {
        std::cout << "Wrong command" << std::endl;
        return false;
    }

    switch (argv[2][1])
    {
    case 'a':
    {
        std::string s(argv[3]);
        FAT::extractFilename(s);
        if (s.length() >= 13)
        {
            std::cout << "Maximum name length is 12 characters" << std::endl;
            return false;
        }
        if (strncmp(argv[3], "ffffffff", 8)==0)
        {
            std::cout << "Name ffffffff for file is forbidden!" << std::endl;
            return false;
        }
        // Check if arguments are <fatfile> <command> <name> <path>
        if (argc != 5)
        {
            std::cout << "Not enough arguments for command (expected 4)" << std::endl;
            std::cout << "Correct syntax is <fatfile> <command> <name> <path>" << std::endl;
            return false;
        }
        break;
    }
    case 'm':
        if (strlen(argv[3]) >= 13)
        {
            std::cout << "Maximum name length is 12 characters" << std::endl;
            return false;
        }
        if (strncmp(argv[3], "ffffffff", 8)==0)
        {
            std::cout << "Name ffffffff for directory is forbidden!" << std::endl;
            return false;
        }
        // Check if arguments are <fatfile> <command> <name> <path>
        if (argc != 5)
        {
            std::cout << "Not enough arguments for command (expected 4)" << std::endl;
            std::cout << "Correct syntax is <fatfile> <command> <name> <path>" << std::endl;
            return false;
        }
        break;
    case 'f':
    case 'c':
    case 'r':
    case 'l':
        // Check if arguments are <fatfile> <command> <path>
        if (argc != 4)
        {
            std::cout << "Not enough arguments for command (expected 3)" << std::endl;
            std::cout << "Correct syntax is <fatfile> <command> <path>" << std::endl;
            return false;
        }
        break;
    case 'p':
        // Check if arguments are <fatfile> <command>
        if (argc != 3)
        {
            std::cout << "Not enough arguments for command (expected 2)" << std::endl;
            std::cout << "Correct syntax is <fatfile> <command>" << std::endl;
            return false;
        }
        break;
    // Testing command that i used for calculate load times for different thread numbers
    case 't':
        FAT::max_threads = std::max(atoi(argv[3]), 1);
        break;
    case 'x':
        break;
    case 'b':
        if (argc != 4 || atoi(argv[3]) < 0)
        {
            std::cout << "Not enough arguments for command (expected 3)" << std::endl;
            std::cout << "Correct syntax is <fatfile> <command> <cluster number>" << std::endl;
            return false;
        }
        break;
    default:
        std::cout << "Available commands:" << std::endl;
        std::cout << "-a for adding new file to fat" << std::endl;
        std::cout << "-m creating new dir in fat" << std::endl;
        std::cout << "-p for print filesystem" << std::endl;
        std::cout << "-f remove file from fat" << std::endl;
        std::cout << "-r remove dir from fat" << std::endl;
        std::cout << "-c print clusters of file" << std::endl;
        std::cout << "-l print content of file" << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) 
{
    // Seed randomizer
    std::srand((unsigned int)time(NULL));
    // Initialize max_threads to default value
    FAT::max_threads = THREADS;

    // Validate arguments
    if (!validateArguments(argc, argv))
        return 0;
    try
    {
        // Load fat file
        FAT fat(argv[1]);

        switch (argv[2][1])
        {
            case 'a':
                // Load and add file argv[3] int fat on path argv[4]
                fat.addFile(argv[3], argv[4]);
                break;
            case 'm':
                // Create dir named argv[3] in path argv[4]
                fat.createDir(argv[3], argv[4]);
                break;
            case 'f':
                // Remove file argv[3] from fat
                fat.remove(argv[3], FAT_FILE_END);
                break;
            case 'r':
                // Remove dir argv[3] from fat
                fat.remove(argv[3], FAT_DIRECTORY);
                break;
            case 'c':
                // Print list of file argv[3] clusters
                fat.printFileClusters(argv[3]);
                break;
            case 'l':
                // Print file argv[3] content
                fat.printFile(argv[3]);
                break;
            case 'p':
                // Print file structure of fat
                fat.printFat();
                break;
            case 'x':
                fat.printFirstFewFatRows();
                break;
            case 'b':
                fat.corruptCluster(atoi(argv[3]));
                break;
        }
    }
    // Print error if occurred
    catch(std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    
}
