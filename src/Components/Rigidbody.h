#pragma once
#include "Component.h"
#include <glm/glm.hpp>

class Rigidbody : public Component
{
public:
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 acceleration = glm::vec3(0.0f);
    float mass = 1.0f;
    float invMass = 1.0f;
    bool useGravity = true;
    bool isKinematic = false;

    Rigidbody() { updateInvMass(); }

    explicit Rigidbody(float mass, bool useGravity = true, bool isKinematic = false)
        : mass(mass), useGravity(useGravity), isKinematic(isKinematic)
    { updateInvMass(); }

    void updateInvMass() 
    {
        if (mass > 0.0f && !isKinematic) 
        {
            invMass = 1.0f / mass;
        }
        else 
        {
            invMass = 0.0f;
        }
    }

    void addForce(const glm::vec3& force) 
    {
        if (!isKinematic && invMass > 0.0f) 
        {
            acceleration += force * invMass;
        }
    }
};