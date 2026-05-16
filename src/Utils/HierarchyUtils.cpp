#include "Utils/HierarchyUtils.h"

void HierarchyUtils::setParent(World* world, EntityId child, EntityId parent)
{
    if (child == parent) return;

    removeFromParent(world, child);

    if (parent != -1) 
    {
        if (!world->hasComponent<Hierarchy>(parent)) 
        {
            world->addComponent<Hierarchy>(parent, Hierarchy{});
        }

        Hierarchy* parentHierarchy = world->getComponent<Hierarchy>(parent);
        parentHierarchy->addChild(child);

        if (!world->hasComponent<Hierarchy>(child)) 
        {
            world->addComponent<Hierarchy>(child, Hierarchy{});
        }

        Hierarchy* childHierarchy = world->getComponent<Hierarchy>(child);
        childHierarchy->parent = parent;
    }

    if (world->hasComponent<Transform>(child))
    {
        Transform* transform = world->getComponent<Transform>(child);
        transform->markDirty();
    }
}

void HierarchyUtils::removeFromParent(World* world, EntityId child)
{
    if (!world->hasComponent<Hierarchy>(child)) return;

    Hierarchy* childHierarchy = world->getComponent<Hierarchy>(child);
    EntityId oldParent = childHierarchy->parent;

    if (oldParent != -1 && world->hasComponent<Hierarchy>(oldParent)) {
        Hierarchy* parentHierarchy = world->getComponent<Hierarchy>(oldParent);
        parentHierarchy->removeChild(child);
    }

    childHierarchy->parent = -1;

    if (world->hasComponent<Transform>(child)) {
        Transform* transform = world->getComponent<Transform>(child);
        transform->markDirty();
    }
}

std::vector<EntityId> HierarchyUtils::getChildren(World* world, EntityId entity)
{
    if (!world->hasComponent<Hierarchy>(entity)) return {};

    Hierarchy* hierarchy = world->getComponent<Hierarchy>(entity);
    return hierarchy->children;
}


EntityId HierarchyUtils::getParent(World* world, EntityId entity)
{
    if (!world->hasComponent<Hierarchy>(entity)) return -1;

    Hierarchy* hierarchy = world->getComponent<Hierarchy>(entity);
    return hierarchy->parent;
}

void HierarchyUtils::markChildrenDirty(World* world, EntityId entity)
{
    if (!world->hasComponent<Hierarchy>(entity)) return;

    Hierarchy* hierarchy = world->getComponent<Hierarchy>(entity);
    for (EntityId child : hierarchy->children) 
    {
        if (world->hasComponent<Transform>(child)) 
        {
            Transform* childTransform = world->getComponent<Transform>(child);
            childTransform->markDirty();
            markChildrenDirty(world, child);
        }
    }
}
