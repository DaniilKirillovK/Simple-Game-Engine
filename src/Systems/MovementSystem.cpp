#include "MovementSystem.h"
#include "Components/Tag.h"
#include "Components/Transform.h"
#include "World.h"

void MovementSystem::update(World& world, float deltaTime)
{
    {
        static float time = 0.0f;
        time += deltaTime;

        auto tags = world.getEntitiesWithComponent<Tag>();
        for (auto& [entity, tag] : tags) 
        {
            if (tag->name == "Rotating") 
            {
                Transform* transform = world.getComponent<Transform>(entity);
                if (transform) 
                {
                    transform->eulerRotation.y += 45.0f * deltaTime;
                    transform->eulerRotation.x += 30.0f * deltaTime;
                    transform->updateQuaternion();
                }
            }

            if (tag->name == "Bouncing")
            {
                Transform* transform = world.getComponent<Transform>(entity);
                if (transform) 
                {
                    float offset = sinf(time * 3.0f) * 1.5f;
                    transform->position.y = 0.0f + offset;
                }
            }

            if (tag->name == "Orbiting")
            {
                Transform* transform = world.getComponent<Transform>(entity);
                if (transform) 
                {
                    float radius = 3.0f;
                    float speed = 2.0f;
                    transform->position.x = sinf(time * speed) * radius;
                    transform->position.z = cosf(time * speed) * radius;
                }
            }
        }
    }
}
