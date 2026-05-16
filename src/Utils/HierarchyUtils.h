#pragma once
#include "World.h"
#include "Components/Transform.h"
#include "Components/Hierarchy.h"

class HierarchyUtils
{
public:
    static void setParent(World* world, EntityId child, EntityId parent);
    static void removeFromParent(World* world, EntityId child);
    static std::vector<EntityId> getChildren(World* world, EntityId entity);
    static EntityId getParent(World* world, EntityId entity);
    static void markChildrenDirty(World* world, EntityId entity);
};