#include "TransformSystem.h"
#include "Components/Transform.h"
#include "Components/Hierarchy.h"
#include <queue>

#include "tracy/Tracy.hpp"

void TransformSystem::update(World& world, JobSystem* jobs, float deltaTime)
{
    ZoneScopedN("TransformSystem::update");

    if (!m_isEnabled) return;

    updateWorldMatrices(world);
}

void TransformSystem::setEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}

void TransformSystem::updateWorldMatrices(World& world)
{
    auto& tPool = world.getComponentPool<Transform>();
    auto& hPool = world.getComponentPool<Hierarchy>();

    auto& transforms = tPool.components();
    auto& transEnts = tPool.entities();

    for (auto& t : transforms)
    {
        t.markDirty();
    }

    std::queue<EntityId> queue;

    for (size_t i = 0; i < transEnts.size(); ++i)
    {
        EntityId e = transEnts[i];
        Hierarchy* h = hPool.getComponent(e);

        if (!h || h->parent == INVALID_ENTITY)
        {
            queue.push(e);
        }
    }

    while (!queue.empty())
    {
        EntityId current = queue.front();
        queue.pop();

        Transform* transform = tPool.getComponent(current);
        if (!transform) continue;

        glm::mat4 parentMatrix = glm::mat4(1.0f);

        Hierarchy* hierarchy = hPool.getComponent(current);

        if (hierarchy && hierarchy->parent != INVALID_ENTITY)
        {
            Transform* parentTransform = tPool.getComponent(hierarchy->parent);
            if (parentTransform && !parentTransform->worldMatrixDirty)
            {
                parentMatrix = parentTransform->worldMatrix;
            }
        }

        transform->updateWorldMatrix(parentMatrix);

        if (hierarchy)
        {
            for (EntityId child : hierarchy->children)
            {
                queue.push(child);
            }
        }
    }
}
