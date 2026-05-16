#include "TransformSystem.h"
#include "Components/Transform.h"
#include "Components/Hierarchy.h"
#include <queue>

void TransformSystem::update(World& world, float deltaTime)
{
    updateWorldMatrices(world);
}

void TransformSystem::updateWorldMatrices(World& world)
{
    auto& transforms = world.getComponentPool<Transform>();
    auto& hierarchies = world.getComponentPool<Hierarchy>();

    for (auto& [entity, transform] : transforms.getAll())
    {
        transform.markDirty();
    }

    std::queue<EntityId> queue;

    for (auto& [entity, transform] : transforms.getAll())
    {
        bool hasParent = false;
        if (hierarchies.hasComponent(entity))
        {
            Hierarchy* hierarchy = hierarchies.getComponent(entity);
            if (hierarchy->parent != -1 && transforms.hasComponent(hierarchy->parent))
            {
                hasParent = true;
            }
        }
        if (!hasParent)
        {
            queue.push(entity);
        }
    }

    while (!queue.empty())
    {
        EntityId current = queue.front();
        queue.pop();

        Transform* transform = transforms.getComponent(current);
        if (!transform) continue;

        glm::mat4 parentMatrix = glm::mat4(1.0f);

        if (hierarchies.hasComponent(current))
        {
            Hierarchy* hierarchy = hierarchies.getComponent(current);
            if (hierarchy->parent != -1 && transforms.hasComponent(hierarchy->parent))
            {
                Transform* parentTransform = transforms.getComponent(hierarchy->parent);
                if (parentTransform && !parentTransform->worldMatrixDirty)
                {
                    parentMatrix = parentTransform->worldMatrix;
                }
            }
        }

        transform->updateWorldMatrix(parentMatrix);

        if (hierarchies.hasComponent(current))
        {
            Hierarchy* hierarchy = hierarchies.getComponent(current);
            for (EntityId child : hierarchy->children)
            {
                queue.push(child);
            }
        }
    }
}
