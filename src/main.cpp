#include <iostream>
#include "FAT.h"
#include <cstring>
#include <algorithm>
#include <random>

bool validateArguments(int argc, char *argv[])
{
    // Arguments must compose from <path to fat file> <command>
    if (argc < 3)
    {
        std::cout << "Not enough arguments." << std::endl;
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
    case 'm':
        // Check if arguments are <fatfile> <command> <name> <path>
        if (argc != 5)
        {
            std::cout << "Not enough arguments for command (expected 4)" << std::endl;
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
            return false;
        }
        break;
    case 'p':
        // Check if arguments are <fatfile> <command> <name>
        if (argc != 3)
        {
            std::cout << "Not enough arguments for command (expected 2)" << std::endl;
            return false;
        }
        break;
    // Testing command that i used for calculate load times for different thread numbers
    case 't':
        FAT::max_threads = std::max(atoi(argv[3]), 1);
        break;
    default:
        std::cout << "Unknown command" << std::endl;
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
        return 1;
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
        }
    }
    // Print error if occurred
    catch(std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    
}
