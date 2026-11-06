#include "PhysicsSystem.h"
#include "PhysicsSystem.h"
#include "World.h"
#include "Components/Transform.h"
#include "Components/Rigidbody.h"
#include "Components/Collider.h"
#include "Components/Camera.h"
#include "Components/MeshRenderer.h"
#include "Utils/EntityBounds.h"
#include <algorithm>
#include "Logger.h"
#include "InputHandler.h"
#include <cmath>

#include "tracy/Tracy.hpp"

#define PHYSICS_PARALLEL
//#define EXTRA_PHYSICS_CALCULATIONS

namespace 
{
    struct GravityCtx
    {
        PhysicsSystem::PhysicsEntity* entities;
        glm::vec3 gravity;
        float     dt;
    };

    void applyGravityJob(void* raw, uint32_t i)
    {
        auto* ctx = static_cast<GravityCtx*>(raw);
        auto& rb = *ctx->entities[i].rigidbody;

        if (rb.useGravity && !rb.isKinematic)
        {
            rb.velocity += ctx->gravity * ctx->dt;
        }

#ifdef EXTRA_PHYSICS_CALCULATIONS
        int counter = 0;
        for (int i = 0; i < 100000; ++i)
        {
            counter++;
        }
#endif // EXTRA_PHYSICS_CALCULATIONS
    }

    struct IntegrateCtx
    {
        PhysicsSystem::PhysicsEntity* entities;
        float dt;
    };

    void integratePositionsJob(void* raw, uint32_t i)
    {
        auto* ctx = static_cast<IntegrateCtx*>(raw);
        auto& e = ctx->entities[i];
        Rigidbody& rb = *e.rigidbody;

        if (rb.isKinematic) return;

        rb.velocity += rb.acceleration * ctx->dt;
        e.transform->position += rb.velocity * ctx->dt;
        rb.acceleration = glm::vec3(0.0f);

#ifdef EXTRA_PHYSICS_CALCULATIONS
        int counter = 0;
        for (int i = 0; i < 100000; ++i)
        {
            counter++;
        }
#endif // EXTRA_PHYSICS_CALCULATIONS
    }

    struct BroadNarrowCtx
    {
        PhysicsSystem::PhysicsEntity* entities;
        uint32_t count;
        uint32_t jobIdx;
        uint32_t numJobs;
        std::vector<PhysicsSystem::Contact>* output;
    };

    void broadNarrowJob(void* raw, uint32_t i)
    {
        auto* ctx = static_cast<BroadNarrowCtx*>(raw);
        auto* ents = ctx->entities;
        const uint32_t N = ctx->count;
        const uint32_t stride = ctx->numJobs;
        auto& out = *ctx->output;

        for (uint32_t i = ctx->jobIdx; i < N; i += stride)
        {
            auto& A = ents[i];

            for (uint32_t j = i + 1; j < N; ++j)
            {
                auto& B = ents[j];

                CollisionInfo info;
                if (!PhysicsSystem::checkCollision(
                    *A.collider, *A.transform,
                    *B.collider, *B.transform,
                    info))
                    continue;

                if (A.collider->isTrigger || B.collider->isTrigger)
                    continue;

                info.entityA = A.id;
                info.entityB = B.id;

                out.push_back({ &A, &B, info });
            }
        }
    }

    struct MultiBroadNarrowCtx
    {
        BroadNarrowCtx* ctxs;
    };

    void broadNarrowMultiJob(void* raw, uint32_t jobIdx)
    {
        auto* mc = static_cast<MultiBroadNarrowCtx*>(raw);
        broadNarrowJob(&mc->ctxs[jobIdx], jobIdx);
    }
}

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
    ZoneScopedN("PhysicsSystem::update");

    if (m_debugRendering && m_renderer)
    {
        renderDebugColliders(world);
    }

    if (!m_isEnabled) return;

    m_triggers.update(world);

    gatherEntities(world);
    if (m_entities.empty())
    {
        for (auto& buffer : m_contactBuffers) buffer.clear();
        return;
    }

    m_accumulator += deltaTime;

    //while (m_accumulator >= m_fixedTimestep)
    {
        ZoneScopedN("Physics::Step");

#ifdef PHYSICS_PARALLEL
        applyGravityParallel(deltaTime);
        updatePositionsParallel(deltaTime);
#else
        applyGravity(deltaTime);
        updatePositions(deltaTime);
#endif // PHYSICS_PARALLEL

#ifdef PHYSICS_PARALLEL
        {
            ZoneScopedN("PhysicsSystem::detectAndResolveCollisionsParallel");
            detectAndResolveCollisionsParallel();
        }
#else
    detectAndResolveCollisions();
#endif // PHYSICS_PARALLEL

        m_accumulator -= m_fixedTimestep;
    }
}

void PhysicsSystem::setEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;

    if (!isEnabled)
    {
        m_triggers.clear();
        for (auto& buffer : m_contactBuffers) buffer.clear();
    }
}

void PhysicsSystem::gatherEntities(World& world)
{
    ZoneScopedN("Physics::GatherEntities");

    auto& rbPool = world.getComponentPool<Rigidbody>();
    auto& tPool = world.getComponentPool<Transform>();
    auto& cPool = world.getComponentPool<Collider>();

    auto& rbs = rbPool.components();
    auto& rbEnts = rbPool.entities();

    m_entities.clear();
    m_entities.reserve(rbs.size());

    for (size_t i = 0; i < rbs.size(); ++i)
    {
        const EntityId e = rbEnts[i];

        Transform* tr = tPool.getComponent(e);
        Collider* col = cPool.getComponent(e);

        if (tr && col)
        {
            m_entities.push_back({ e, tr, &rbs[i], col });
        }
    }
}

void PhysicsSystem::applyGravity(float dt)
{
    ZoneScopedN("Physics::ApplyGravity");

    for (auto& e : m_entities)
    {
        Rigidbody& rb = *e.rigidbody;
        if (rb.useGravity && !rb.isKinematic)
        {
            rb.velocity += m_gravity * dt;
        }

#ifdef EXTRA_PHYSICS_CALCULATIONS
        int counter = 0;
        for (int i = 0; i < 100000; ++i)
        {
            counter++;
        }
#endif // EXTRA_PHYSICS_CALCULATIONS
    }
}

void PhysicsSystem::updatePositions(float dt)
{
    ZoneScopedN("Physics::UpdatePositions");

    for (auto& e : m_entities)
    {
        Rigidbody& rb = *e.rigidbody;
        if (rb.isKinematic) continue;

        rb.velocity += rb.acceleration * dt;
        e.transform->position += rb.velocity * dt;
        rb.acceleration = glm::vec3(0.0f);

#ifdef EXTRA_PHYSICS_CALCULATIONS
        int counter = 0;
        for (int i = 0; i < 100000; ++i)
        {
            counter++;
        }
#endif // EXTRA_PHYSICS_CALCULATIONS
    }
}

void PhysicsSystem::detectAndResolveCollisions()
{
    ZoneScopedN("Physics::DetectAndResolve");

    const size_t n = m_entities.size();

    for (size_t i = 0; i < n; ++i)
    {
        auto& A = m_entities[i];
        for (size_t j = i + 1; j < n; ++j)
        {
            auto& B = m_entities[j];

            CollisionInfo info;
            if (!checkCollision(*A.collider, *A.transform,
                *B.collider, *B.transform, info))
                continue;

            info.entityA = A.id;
            info.entityB = B.id;

            if (A.collider->isTrigger || B.collider->isTrigger) continue;

            resolveCollision(info, *A.rigidbody, *B.rigidbody,
                *A.transform, *B.transform);
        }
    }
}

void PhysicsSystem::detectAndResolveCollisionsParallel()
{
    ZoneScopedN("Physics::DetectAndResolve (parallel)");

    const uint32_t N = static_cast<uint32_t>(m_entities.size());
    if (N < 2) return;

    const uint32_t numJobs = std::max(1u, JobSystem::get().getNumWorkers());

    if (m_contactBuffers.size() < numJobs)
        m_contactBuffers.resize(numJobs);

    std::vector<BroadNarrowCtx> ctxs(numJobs);
    for (uint32_t k = 0; k < numJobs; ++k)
    {
        m_contactBuffers[k].clear();
        m_contactBuffers[k].reserve(64);
        ctxs[k] = { m_entities.data(), N, k, numJobs, &m_contactBuffers[k] };
    }

    {
        ZoneScopedN("Physics::BroadNarrow (parallel)");
        MultiBroadNarrowCtx mc{ ctxs.data() };
        JobSystem::get().parallelFor(numJobs, 1, broadNarrowMultiJob, &mc,
            "Physics::BroadNarrow");
    }

    {
        ZoneScopedN("Physics::Resolve (serial)");
        for (auto& buf : m_contactBuffers)
        {
            for (auto& c : buf)
            {
                resolveCollision(c.info,
                    *c.a->rigidbody, *c.b->rigidbody,
                    *c.a->transform, *c.b->transform);
            }
        }
    }
}

void PhysicsSystem::applyGravityParallel(float dt)
{
    ZoneScopedN("Physics::Gravity (parallel)");

    if (m_entities.empty()) return;

    GravityCtx ctx{ m_entities.data(), m_gravity, dt };

    const uint32_t count = static_cast<uint32_t>(m_entities.size());
    const uint32_t batch = std::max(64u,
        count / (JobSystem::get().getNumWorkers() * 4));

    JobSystem::get().parallelFor(count, batch, applyGravityJob, &ctx, "Physics::Gravity (parallel)");
}

void PhysicsSystem::updatePositionsParallel(float dt)
{
    ZoneScopedN("Physics::Integrate (parallel)");

    if (m_entities.empty()) return;

    IntegrateCtx ctx{ m_entities.data(), dt };

    const uint32_t count = static_cast<uint32_t>(m_entities.size());
    const uint32_t batch = std::max(64u,
        count / (JobSystem::get().getNumWorkers() * 4));

    JobSystem::get().parallelFor(count, batch, integratePositionsJob, &ctx, "Physics::Integrate (parallel)");
}

void PhysicsSystem::renderDebugColliders(World& world)
{
    if (!m_renderer) return;

    glm::vec3 cameraPosition(0.0f);
    bool hasCamera = false;
    auto& cameras = world.getComponentPool<Camera>();
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        if (!cameras.components()[i].isActive) continue;
        if (const Transform* transform = world.getComponent<Transform>(cameras.entities()[i]))
        {
            cameraPosition = transform->position;
            hasCamera = true;
        }
        break;
    }

    const glm::vec4 touchingColor(0.2f, 1.0f, 0.3f, 0.9f);

    m_renderer->beginDebugDraw();

    auto& colliders = world.getComponentPool<Collider>();
    for (size_t i = 0; i < colliders.size(); ++i)
    {
        const EntityId entity = colliders.entities()[i];
        const Collider& collider = colliders.components()[i];

        Bounds bounds;
        if (!EntityBounds::compute(world, entity, bounds)) continue;

        const bool containsCamera = hasCamera
            && glm::all(glm::greaterThanEqual(cameraPosition, bounds.min))
            && glm::all(glm::lessThanEqual(cameraPosition, bounds.max));
        if (containsCamera && !world.getComponent<MeshRenderer>(entity)) continue;

        glm::vec4 color(0.3f, 0.6f, 1.0f, 0.6f);
        if (m_triggers.isTouching(entity))
        {
            color = touchingColor;
        }
        else if (collider.isTrigger)
        {
            color = glm::vec4(1.0f, 0.85f, 0.1f, 0.6f);
        }
        else if (world.getComponent<Rigidbody>(entity))
        {
            color = glm::vec4(1.0f, 0.2f, 0.2f, 0.6f);
        }

        if (collider.type == ColliderType::Box)
        {
            m_renderer->drawDebugAABB(bounds.min, bounds.max, color);
        }
        else
        {
            m_renderer->drawDebugSphere((bounds.min + bounds.max) * 0.5f, collider.radius + 0.05f, color);
        }
    }

    for (const auto& [trigger, other] : m_triggers.pairs())
    {
        Bounds a;
        Bounds b;
        if (!EntityBounds::compute(world, trigger, a) || !EntityBounds::compute(world, other, b)) continue;

        const glm::vec3 min = glm::max(a.min, b.min);
        const glm::vec3 max = glm::min(a.max, b.max);
        if (glm::all(glm::lessThanEqual(min, max)))
        {
            m_renderer->drawDebugAABB(min - glm::vec3(0.01f), max + glm::vec3(0.01f), touchingColor);
        }
    }

    for (const auto& buffer : m_contactBuffers)
    {
        for (const Contact& contact : buffer)
        {
            const glm::vec3 point = contact.info.contactPoint;
            const float size = 0.12f;
            const glm::vec4 contactColor(1.0f, 1.0f, 1.0f, 1.0f);
            m_renderer->drawDebugLine(point - glm::vec3(size, 0, 0), point + glm::vec3(size, 0, 0), contactColor);
            m_renderer->drawDebugLine(point - glm::vec3(0, size, 0), point + glm::vec3(0, size, 0), contactColor);
            m_renderer->drawDebugLine(point - glm::vec3(0, 0, size), point + glm::vec3(0, 0, size), contactColor);
            m_renderer->drawDebugLine(point, point + contact.info.normal * 0.5f, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
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
