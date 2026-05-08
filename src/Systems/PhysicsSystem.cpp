#include "PhysicsSystem.h"
#include "PhysicsSystem.h"
#include "World.h"
#include "Components/Transform.h"
#include "Components/Rigidbody.h"
#include "Components/Collider.h"
#include <algorithm>
#include "Logger.h"
#include <cmath>

PhysicsSystem::PhysicsSystem(IRenderAdapter* renderer)
    : m_renderer(renderer) {}

void PhysicsSystem::update(World& world, float deltaTime)
{
    std::vector<EntityId> entities;

    auto rigidbodyEntities = world.getEntitiesWithComponent<Rigidbody>();

    for (auto& [entity, rb] : rigidbodyEntities)
    {
        if (world.hasComponent<Transform>(entity) && world.hasComponent<Collider>(entity))
        {
            entities.push_back(entity);
        }
    }

    if (entities.empty()) return;

    m_accumulator += deltaTime;

    while (m_accumulator >= m_fixedTimestep) 
    {
        applyGravity(world, entities, m_fixedTimestep);
        updatePositions(world, entities, m_fixedTimestep);
        detectAndResolveCollisions(world, entities);

        m_accumulator -= m_fixedTimestep;
    }

    if (m_debugRendering && m_renderer) 
    {
        renderDebugColliders(world);
    }
}

void PhysicsSystem::applyGravity(World& world, std::vector<EntityId>& entities, float dt)
{
    for (EntityId entity : entities) 
    {
        Rigidbody* rb = world.getComponent<Rigidbody>(entity);

        if (rb && rb->useGravity && !rb->isKinematic) 
        {
            rb->velocity += m_gravity * dt;
        }
    }
}

void PhysicsSystem::updatePositions(World& world, std::vector<EntityId>& entities, float dt)
{
    for (EntityId entity : entities) 
    {
        Transform* transform = world.getComponent<Transform>(entity);
        Rigidbody* rb = world.getComponent<Rigidbody>(entity);

        if (transform && rb && !rb->isKinematic) 
        {
            rb->velocity += rb->acceleration * dt;
            transform->position += rb->velocity * dt;
            rb->acceleration = glm::vec3(0.0f);
        }
    }
}

void PhysicsSystem::detectAndResolveCollisions(World& world, std::vector<EntityId>& entities)
{
    for (size_t i = 0; i < entities.size(); ++i) 
    {
        for (size_t j = i + 1; j < entities.size(); ++j) 
        {
            EntityId entityA = entities[i];
            EntityId entityB = entities[j];

            Collider* colliderA = world.getComponent<Collider>(entityA);
            Collider* colliderB = world.getComponent<Collider>(entityB);
            Transform* transformA = world.getComponent<Transform>(entityA);
            Transform* transformB = world.getComponent<Transform>(entityB);

            if (!colliderA || !colliderB || !transformA || !transformB) continue;

            CollisionInfo info;

            if (checkCollision(*colliderA, *transformA, *colliderB, *transformB, info)) 
            {
                info.entityA = entityA;
                info.entityB = entityB;

                if (!colliderA->isTrigger && !colliderB->isTrigger) 
                {
                    Rigidbody* rbA = world.getComponent<Rigidbody>(entityA);
                    Rigidbody* rbB = world.getComponent<Rigidbody>(entityB);

                    resolveCollision(info, *rbA, *rbB, *transformA, *transformB);
                }
            }
        }
    }
}

void PhysicsSystem::renderDebugColliders(World& world)
{
    if (!m_renderer) return;

    m_renderer->beginDebugDraw();

    auto colliderEntities = world.getEntitiesWithComponent<Collider>();

    for (auto& [entity, collider] : colliderEntities)
    {
        if (world.hasComponent<Transform>(entity))
        {
            const Transform* transform = world.getComponent<Transform>(entity);
            if (transform) 
            {
                glm::vec3 worldMin = collider->getWorldMin(transform->position);
                glm::vec3 worldMax = collider->getWorldMax(transform->position);

                glm::vec4 color = collider->isTrigger
                    ? glm::vec4(1.0f, 1.0f, 0.0f, 0.5f)
                    : glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);

                m_renderer->drawDebugAABB(worldMin, worldMax, color);
            }
        }
    }

    m_renderer->endDebugDraw();
}

bool PhysicsSystem::checkCollision(const Collider& a, const Transform& transformA, const Collider& b, const Transform& transformB, CollisionInfo& outInfo)
{
    if (a.type == ColliderType::Box && b.type == ColliderType::Box) 
    {
        glm::vec3 minA = a.getWorldMin(transformA.position);
        glm::vec3 maxA = a.getWorldMax(transformA.position);
        glm::vec3 minB = b.getWorldMin(transformB.position);
        glm::vec3 maxB = b.getWorldMax(transformB.position);

        if (maxA.x < minB.x || minA.x > maxB.x) return false;
        if (maxA.y < minB.y || minA.y > maxB.y) return false;
        if (maxA.z < minB.z || minA.z > maxB.z) return false;

        glm::vec3 centerA = (minA + maxA) * 0.5f;
        glm::vec3 centerB = (minB + maxB) * 0.5f;
        glm::vec3 delta = centerB - centerA;

        glm::vec3 overlap;
        overlap.x = (maxA.x - minA.x) * 0.5f + (maxB.x - minB.x) * 0.5f - std::abs(delta.x);
        overlap.y = (maxA.y - minA.y) * 0.5f + (maxB.y - minB.y) * 0.5f - std::abs(delta.y);
        overlap.z = (maxA.z - minA.z) * 0.5f + (maxB.z - minB.z) * 0.5f - std::abs(delta.z);

        if (overlap.x < overlap.y && overlap.x < overlap.z) 
        {
            outInfo.normal = glm::vec3(delta.x > 0 ? 1 : -1, 0, 0);
            outInfo.penetrationDepth = overlap.x;
        }
        else if (overlap.y < overlap.x && overlap.y < overlap.z) 
        {
            outInfo.normal = glm::vec3(0, delta.y > 0 ? 1 : -1, 0);
            outInfo.penetrationDepth = overlap.y;
        }
        else 
        {
            outInfo.normal = glm::vec3(0, 0, delta.z > 0 ? 1 : -1);
            outInfo.penetrationDepth = overlap.z;
        }

        outInfo.contactPoint = (centerA + centerB) * 0.5f;

        return true;
    }

    // Sphere vs Sphere
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Sphere) 
    {
        glm::vec3 centerA = a.getCenter(transformA.position);
        glm::vec3 centerB = b.getCenter(transformB.position);
        glm::vec3 delta = centerB - centerA;
        float distance = glm::length(delta);
        float radiusSum = a.radius + b.radius;

        if (distance < radiusSum) 
        {
            outInfo.normal = glm::normalize(delta);
            outInfo.penetrationDepth = radiusSum - distance;
            outInfo.contactPoint = centerA + outInfo.normal * a.radius;
            return true;
        }
        return false;
    }

    // Box vs Sphere 
    if (a.type == ColliderType::Box && b.type == ColliderType::Sphere) 
    {
        glm::vec3 minA = a.getWorldMin(transformA.position);
        glm::vec3 maxA = a.getWorldMax(transformA.position);
        glm::vec3 centerB = b.getCenter(transformB.position);
        float radiusB = b.radius;

        glm::vec3 closestPoint;
        closestPoint.x = glm::clamp(centerB.x, minA.x, maxA.x);
        closestPoint.y = glm::clamp(centerB.y, minA.y, maxA.y);
        closestPoint.z = glm::clamp(centerB.z, minA.z, maxA.z);

        glm::vec3 delta = centerB - closestPoint;
        float distanceSq = glm::dot(delta, delta);
        float radiusSq = radiusB * radiusB;

        if (distanceSq < radiusSq) 
        {
            float distance = sqrt(distanceSq);
            outInfo.normal = glm::normalize(delta);
            outInfo.penetrationDepth = radiusB - distance;
            outInfo.contactPoint = closestPoint;
            return true;
        }
        return false;
    }

    // Sphere vs Box
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Box) 
    {
        return checkCollision(b, transformB, a, transformA, outInfo);
    }

    return false;
}

void PhysicsSystem::resolveCollision(CollisionInfo& info, Rigidbody& rbA, Rigidbody& rbB, Transform& transformA, Transform& transformB)
{
    float totalInvMass = rbA.invMass + rbB.invMass;
    if (totalInvMass > 0.0f) 
    {
        float correctionA = info.penetrationDepth * (rbA.invMass / totalInvMass);
        float correctionB = info.penetrationDepth * (rbB.invMass / totalInvMass);

        transformA.position -= info.normal * correctionA;
        transformB.position += info.normal * correctionB;
    }

    glm::vec3 relativeVelocity = rbB.velocity - rbA.velocity;
    float velocityAlongNormal = glm::dot(relativeVelocity, info.normal);

    if (velocityAlongNormal < 0.0f) 
    {
        float restitution = 0.5f;
        float impulseMagnitude = (1.0f + restitution) * velocityAlongNormal / totalInvMass;
        glm::vec3 impulse = info.normal * impulseMagnitude;

        if (!rbA.isKinematic) 
        {
            rbA.velocity += impulse * rbA.invMass;
        }
        if (!rbB.isKinematic) 
        {
            rbB.velocity -= impulse * rbB.invMass;
        }
    }
}
