#include "Utils/HierarchyUtils.h"

void HierarchyUtils::setParent(World* world, EntityId child, EntityId parent)
{
    if (child == parent) return;

    removeFromParent(world, child);

    if (parent != -1) 
    {
        if (!world->getComponentPool<Hierarchy>().hasComponent(parent))
        {
            world->getComponentPool<Hierarchy>().addComponent(parent, Hierarchy{});
        }

        Hierarchy* parentHierarchy = world->getComponent<Hierarchy>(parent);
        parentHierarchy->addChild(child);

        if (!world->getComponentPool<Hierarchy>().hasComponent(child))
        {
            world->getComponentPool<Hierarchy>().addComponent(child, Hierarchy{});
        }

        Hierarchy* childHierarchy = world->getComponent<Hierarchy>(child);
        childHierarchy->parent = parent;
    }

    if (world->getComponentPool<Transform>().hasComponent(child))
    {
        Transform* transform = world->getComponent<Transform>(child);
        transform->markDirty();
    }
}

void HierarchyUtils::removeFromParent(World* world, EntityId child)
{
    if (!world->getComponentPool<Hierarchy>().hasComponent(child)) return;

    Hierarchy* childHierarchy = world->getComponent<Hierarchy>(child);
    EntityId oldParent = childHierarchy->parent;

    if (oldParent != -1 && world->getComponentPool<Hierarchy>().hasComponent(oldParent))
    {
        Hierarchy* parentHierarchy = world->getComponent<Hierarchy>(oldParent);
        parentHierarchy->removeChild(child);
    }

    childHierarchy->parent = -1;

    if (world->getComponentPool<Transform>().hasComponent(child))
    {
        Transform* transform = world->getComponent<Transform>(child);
        transform->markDirty();
    }
}

std::vector<EntityId> HierarchyUtils::getChildren(World* world, EntityId entity)
{
    if (!world->getComponentPool<Hierarchy>().hasComponent(entity)) return {};

    Hierarchy* hierarchy = world->getComponent<Hierarchy>(entity);
    return hierarchy->children;
}


EntityId HierarchyUtils::getParent(World* world, EntityId entity)
{
    if (!world->getComponentPool<Hierarchy>().hasComponent(entity)) return -1;

    Hierarchy* hierarchy = world->getComponent<Hierarchy>(entity);
    return hierarchy->parent;
}

void HierarchyUtils::markChildrenDirty(World* world, EntityId entity)
{
    if (!world->getComponentPool<Hierarchy>().hasComponent(entity)) return;

    Hierarchy* hierarchy = world->getComponent<Hierarchy>(entity);
    for (EntityId child : hierarchy->children) 
    {
        if (world->getComponentPool<Transform>().hasComponent(child))
        {
            Transform* childTransform = world->getComponent<Transform>(child);
            childTransform->markDirty();
            markChildrenDirty(world, child);
        }
    }
}
