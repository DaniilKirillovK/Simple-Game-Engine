#pragma once
#include "Component.h"
#include <glm/glm.hpp>

enum class ColliderType 
{
    Box,
    Sphere
};

class Collider : public Component
{
public:
    ColliderType type = ColliderType::Box;
    
    // For AABB
    glm::vec3 halfSize = glm::vec3(0.5f);

    // Sphere
    float radius = 0.5f;

    glm::vec3 offset = glm::vec3(0.0f);

    float bounciness = 0.5f;
    float friction = 0.5f;

    bool isTrigger = false;

    Collider() = default;

    explicit Collider(const glm::vec3& halfSize)
        : type(ColliderType::Box), halfSize(halfSize) {}

    explicit Collider(float radius)
        : type(ColliderType::Sphere), radius(radius) {}

    glm::vec3 getWorldMin(const glm::vec3& position) const 
    {
        return position + offset - halfSize;
    }

    glm::vec3 getWorldMax(const glm::vec3& position) const 
    {
        return position + offset + halfSize;
    }

    glm::vec3 getCenter(const glm::vec3& position) const 
    {
        return position + offset;
    }
};

