#pragma once
#include "ISystem.h"
#include "IRenderAdapter.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>
#include "Entity.h"

class Transform;
class Rigidbody;
class Collider;


struct CollisionInfo 
{
    EntityId entityA;
    EntityId entityB;
    glm::vec3 normal;
    float penetrationDepth;
    glm::vec3 contactPoint;
};

class PhysicsSystem : public ISystem 
{
public:
    struct PhysicsEntity
    {
        EntityId   id;
        Transform* transform;
        Rigidbody* rigidbody;
        Collider* collider;
    };

    PhysicsSystem(IRenderAdapter* renderAdapter);

    virtual void update(World& world, JobSystem* jobs, float deltaTime) override;
    virtual void setEnabled(bool isEnabled) override;

    void setGravity(const glm::vec3& gravity) { m_gravity = gravity; }
    glm::vec3 getGravity() const { return m_gravity; }

    void setDebugRendering(bool enabled) { m_debugRendering = enabled; }
    bool isDebugRenderingEnabled() const { return m_debugRendering; }
    void toggleDebugRendering() { m_debugRendering = !m_debugRendering; }

private:
    void gatherEntities(World& world);

    void applyGravity(float dt);
    void updatePositions(float dt);
    void detectAndResolveCollisions();

    void applyGravityParallel(JobSystem* jobs, float dt);
    void updatePositionsParallel(JobSystem* jobs, float dt);

    void renderDebugColliders(World& world);

    bool checkCollision(const Collider& a, const Transform& transformA,
        const Collider& b, const Transform& transformB,
        CollisionInfo& outInfo);

    void resolveCollision(CollisionInfo& info,
        Rigidbody& rbA, Rigidbody& rbB,
        Transform& transformA, Transform& transformB);

    std::vector<PhysicsEntity> m_entities;

    glm::vec3 m_gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float m_fixedTimestep = 1.0f / 60.f;
    float m_accumulator = 0.0f;
    int m_maxSubsteps = 5;

    bool m_isEnabled = false;
    bool m_debugRendering = true;
    IRenderAdapter* m_renderer = nullptr;
};

