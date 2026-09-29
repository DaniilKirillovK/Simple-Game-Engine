#include "MovementSystem.h"
#include "Components/Tag.h"
#include "Components/Transform.h"
#include "World.h"

#include "tracy/Tracy.hpp"

void MovementSystem::update(World& world, JobSystem* jobs, float deltaTime)
{
    ZoneScopedN("MovementSystem::update");

    if (!m_isEnabled) return;

    m_time += deltaTime;

    auto& tagPool = world.getComponentPool<Tag>();
    auto& tPool = world.getComponentPool<Transform>();

    auto& tags = tagPool.components();
    auto& ents = tagPool.entities();

    const float bounceOffset = sinf(m_time * 3.0f) * 1.5f;
    const float orbitX = sinf(m_time * 2.0f) * 3.0f;
    const float orbitZ = cosf(m_time * 2.0f) * 3.0f;

    const float rotStepY = 45.0f * deltaTime;
    const float rotStepX = 30.0f * deltaTime;

    for (size_t i = 0; i < tags.size(); ++i)
    {
        Transform* tr = tPool.getComponent(ents[i]);
        if (!tr) continue;

        const std::string& name = tags[i].name;

        if (name == "Rotating")
        {
            tr->eulerRotation.y += rotStepY;
            tr->eulerRotation.x += rotStepX;
            tr->updateQuaternion();
        }
        else if (name == "Bouncing")
        {
            tr->position.y = bounceOffset;
        }
        else if (name == "Orbiting")
        {
            tr->position.x = orbitX;
            tr->position.z = orbitZ;
        }
    }
}

void MovementSystem::setEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}
