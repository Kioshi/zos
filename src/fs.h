#pragma once
#include "util.h"
#include <string>
#include <vector>

class Node
{
public:
    Node(std::string name, int32 cluster, bool isFile, int32 size, Node* _parent);
    ~Node();

    std::string name;
    std::vector<Node*> childs;
    Node* parent;
    bool isFile;
    int32 size;
    int32 cluster;

};
