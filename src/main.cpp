#include <iostream>
#include "FAT.h"

bool validateArguments(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Not enough arguments." << std::endl;
        return false;
    }

    if (strlen(argv[2]) < 2)
    {
        std::cout << "Wrong command" << std::endl;
        return false;
    }

    switch (argv[2][1])
    {
    case 'a':
    case 'm':
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
        if (argc != 4)
        {
            std::cout << "Not enough arguments for command (expected 3)" << std::endl;
            return false;
        }
        break;
    case 'p':
        if (argc != 3)
        {
            std::cout << "Not enough arguments for command (expected 2)" << std::endl;
            return false;
        }
        break;
    default:
        std::cout << "Wrong command" << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) 
{
    if (!validateArguments(argc, argv))
        return 1;
    try
    {
        FAT fat(argv[1]);

        switch (argv[2][1])
        {
        case 'a':
            fat.addFile(argv[3], argv[4]);
            break;
        case 'm':
            fat.createDir(argv[3], argv[4]);
            break;
        case 'f':
            fat.remove(argv[3], FAT_FILE_END);
            break;
        case 'r':
            fat.remove(argv[3], FAT_DIRECTORY);
            break;
        case 'c':
            fat.printFileClusters(argv[3]);
            break;
        case 'l':
            fat.printFile(argv[3]);
            break;
        case 'p':
            fat.printFat();
            break;
        }
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    
}
