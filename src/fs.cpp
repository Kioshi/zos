#include "fs.h"


Node::Node(std::string _name, int32 _cluster, bool _isFile, int32 _size, Node* _parent)
    : name(_name)
    , cluster(_cluster)
    , isFile(_isFile)
    , size(_size)
    , parent(_parent)
{

}

Node::~Node()
{
    for (auto node : childs)
        delete node;
}

// Secure add children from multiple threads
void Node::addChild(Node* child)
{
    Guard guard(_lock);
    childs.push_back(child);
}
