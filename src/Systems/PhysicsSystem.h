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
    PhysicsSystem(IRenderAdapter* renderAdapter);

    void update(World& world, float deltaTime) override;

    void setGravity(const glm::vec3& gravity) { m_gravity = gravity; }
    glm::vec3 getGravity() const { return m_gravity; }

    void setDebugRendering(bool enabled) { m_debugRendering = enabled; }
    bool isDebugRenderingEnabled() const { return m_debugRendering; }
    void toggleDebugRendering() { m_debugRendering = !m_debugRendering; }

private:
    IRenderAdapter* m_renderer;

    glm::vec3 m_gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    bool m_debugRendering = false;

    float m_fixedTimestep = 1.0f / 120.f;
    float m_accumulator = 0.0f;

    void applyGravity(World& world, std::vector<EntityId>& entities, float dt);
    void updatePositions(World& world, std::vector<EntityId>& entities, float dt);
    void detectAndResolveCollisions(World& world, std::vector<EntityId>& entities);

    void renderDebugColliders(World& world);

    bool checkCollision(const Collider& a, const Transform& transformA,
        const Collider& b, const Transform& transformB,
        CollisionInfo& outInfo);

    void resolveCollision(CollisionInfo& info,
        Rigidbody& rbA, Rigidbody& rbB,
        Transform& transformA, Transform& transformB);
};