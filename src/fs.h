#pragma once
#include "util.h"
#include <string>
#include <vector>


class Node
{
public:
    Node(std::string name, int32 cluster, bool isFile, int32 size, Node* _parent);
    ~Node();
    void addChild(Node* child);

    std::string name;
    Node* parent;
    bool isFile;
    int32 size;
    int32 cluster;
    std::vector<Node*> childs;
private:
    std::mutex _lock;

};
