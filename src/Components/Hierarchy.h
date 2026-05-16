#pragma once
#include "Component.h"
#include "Entity.h"
#include <vector>

class Hierarchy : public Component 
{
public:
    EntityId parent = -1;
    std::vector<EntityId> children;

    Hierarchy() = default;
    Hierarchy(EntityId parentId) : parent(parentId) {}

    void addChild(EntityId child) 
    {
        children.push_back(child);
    }

    void removeChild(EntityId child) 
    {
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end()) 
        {
            children.erase(it);
        }
    }

    bool hasChildren() const 
    {
        return !children.empty();
    }

    bool isRoot() const 
    {
        return parent == -1;
    }
};