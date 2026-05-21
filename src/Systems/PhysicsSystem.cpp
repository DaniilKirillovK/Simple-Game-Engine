#include "PhysicsSystem.h"
#include "PhysicsSystem.h"
#include "World.h"
#include "Components/Transform.h"
#include "Components/Rigidbody.h"
#include "Components/Collider.h"
#include <algorithm>
#include "Logger.h"
#include "InputHandler.h"
#include <cmath>

PhysicsSystem::PhysicsSystem(IRenderAdapter* renderer)
    : m_renderer(renderer) 
{
    m_renderer->setOnToggleDebugCallback([this](bool enabled) {
        if (this) 
        {
            setDebugRendering(enabled);
            LOG_INFO("Debug colliders: " + std::string(enabled ? "ON" : "OFF"));
        }
    });
}

void PhysicsSystem::update(World& world, float deltaTime)
{
    if (m_debugRendering && m_renderer)
    {
        renderDebugColliders(world);
    }

    if (!m_isEnabled) return;

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
}

void PhysicsSystem::setEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
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
                glm::vec3 worldMin = collider->getWorldMin(transform->getWorldPosition());
                glm::vec3 worldMax = collider->getWorldMax(transform->getWorldPosition());

                glm::vec4 color = collider->isTrigger
                    ? glm::vec4(1.0f, 1.0f, 0.0f, 0.5f)
                    : glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);

                if (collider->type == ColliderType::Box)
                {
                    m_renderer->drawDebugAABB(worldMin, worldMax, color);
                }
                else if (collider->type == ColliderType::Sphere)
                {
                    m_renderer->drawDebugSphere(transform->getWorldPosition(), collider->radius + 0.05f, color);
                }
            }
        }
    }

    m_renderer->endDebugDraw();
}

bool PhysicsSystem::checkCollision(const Collider& a, const Transform& transformA, const Collider& b, const Transform& transformB, CollisionInfo& outInfo)
{
    if (a.type == ColliderType::Box && b.type == ColliderType::Box) 
    {
        glm::vec3 minA = a.getWorldMin(transformA.getWorldPosition());
        glm::vec3 maxA = a.getWorldMax(transformA.getWorldPosition());
        glm::vec3 minB = b.getWorldMin(transformB.getWorldPosition());
        glm::vec3 maxB = b.getWorldMax(transformB.getWorldPosition());

        if (maxA.x < minB.x || minA.x > maxB.x) return false;
        if (maxA.y < minB.y || minA.y > maxB.y) return false;
        if (maxA.z < minB.z || minA.z > maxB.z) return false;

        glm::vec3 centerA = (minA + maxA) * 0.5f;
        glm::vec3 centerB = (minB + maxB) * 0.5f;
        glm::vec3 delta = centerB - centerA;

        glm::vec3 halfA = (maxA - minA) * 0.5f;
        glm::vec3 halfB = (maxB - minB) * 0.5f;

        float overlapX = halfA.x + halfB.x - std::abs(delta.x);
        float overlapY = halfA.y + halfB.y - std::abs(delta.y);
        float overlapZ = halfA.z + halfB.z - std::abs(delta.z);

        if (overlapX <= overlapY && overlapX <= overlapZ)
        {
            outInfo.normal = glm::vec3((delta.x > 0) ? 1.0f : -1.0f, 0.0f, 0.0f);
            outInfo.penetrationDepth = overlapX;
        }
        else if (overlapY <= overlapX && overlapY <= overlapZ)
        {
            outInfo.normal = glm::vec3(0.0f, (delta.y > 0) ? 1.0f : -1.0f, 0.0f);
            outInfo.penetrationDepth = overlapY;
        }
        else
        {
            outInfo.normal = glm::vec3(0.0f, 0.0f, (delta.z > 0) ? 1.0f : -1.0f);
            outInfo.penetrationDepth = overlapZ;
        }

        outInfo.contactPoint = (centerA + centerB) * 0.5f;

        return true;
    }

    // Sphere vs Sphere
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Sphere) 
    {
        glm::vec3 centerA = a.getCenter(transformA.getWorldPosition());
        glm::vec3 centerB = b.getCenter(transformB.getWorldPosition());
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
        glm::vec3 boxMin = a.getWorldMin(transformA.getWorldPosition());
        glm::vec3 boxMax = a.getWorldMax(transformA.getWorldPosition());
        glm::vec3 sphereCenter = b.getCenter(transformB.getWorldPosition());
        float sphereRadius = b.radius;

        glm::vec3 closestPoint;
        closestPoint.x = std::max(boxMin.x, std::min(sphereCenter.x, boxMax.x));
        closestPoint.y = std::max(boxMin.y, std::min(sphereCenter.y, boxMax.y));
        closestPoint.z = std::max(boxMin.z, std::min(sphereCenter.z, boxMax.z));

        glm::vec3 delta = sphereCenter - closestPoint;
        float distanceSq = glm::dot(delta, delta);
        float radiusSq = sphereRadius * sphereRadius;

        if (distanceSq < radiusSq)
        {
            float distance = std::sqrt(distanceSq);

            if (distance > 0.0001f)
            {
                outInfo.normal = delta / distance;
            }
            else
            {
                float dx = sphereCenter.x - boxMin.x;
                float dy = sphereCenter.y - boxMin.y;
                float dz = sphereCenter.z - boxMin.z;
                float halfX = (boxMax.x - boxMin.x) * 0.5f;
                float halfY = (boxMax.y - boxMin.y) * 0.5f;
                float halfZ = (boxMax.z - boxMin.z) * 0.5f;

                glm::vec3 centerBox = (boxMin + boxMax) * 0.5f;
                glm::vec3 localPos = sphereCenter - centerBox;

                if (std::abs(localPos.x) > std::abs(localPos.y) &&
                    std::abs(localPos.x) > std::abs(localPos.z))
                {
                    outInfo.normal = glm::vec3(glm::sign(localPos.x), 0, 0);
                }
                else if (std::abs(localPos.y) > std::abs(localPos.x) &&
                    std::abs(localPos.y) > std::abs(localPos.z))
                {
                    outInfo.normal = glm::vec3(0, glm::sign(localPos.y), 0);
                }
                else
                {
                    outInfo.normal = glm::vec3(0, 0, glm::sign(localPos.z));
                }
            }

            outInfo.penetrationDepth = sphereRadius - distance;
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
    if (totalInvMass <= 0.0f) return;

    glm::vec3 normal = glm::normalize(info.normal);

    glm::vec3 delta = transformB.position - transformA.position;
    float dot = glm::dot(normal, delta);

    if (dot < 0)
    {
        normal = -normal;
    }

    float correctionA = info.penetrationDepth * (rbA.invMass / totalInvMass);
    float correctionB = info.penetrationDepth * (rbB.invMass / totalInvMass);

    transformA.position -= normal * correctionA;
    transformB.position += normal * correctionB;

    glm::vec3 relativeVelocity = rbB.velocity - rbA.velocity;
    float velocityAlongNormal = glm::dot(relativeVelocity, normal);

    if (velocityAlongNormal < 0.0f)
    {
        float restitution = 0.5f;
        float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / totalInvMass;
        glm::vec3 impulse = normal * impulseMagnitude;

        rbA.velocity -= impulse * rbA.invMass;
        rbB.velocity += impulse * rbB.invMass;
    }
}
