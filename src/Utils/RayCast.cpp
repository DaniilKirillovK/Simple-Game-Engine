#include "Utils/RayCast.h"
#include "Components/Transform.h"
#include "Utils/EntityBounds.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>


namespace
{
    struct FastRay
    {
        float origin[3];
        float inverse[3];
    };

    FastRay makeFastRay(const Ray& ray)
    {
        FastRay fast;
        for (int axis = 0; axis < 3; ++axis)
        {
            fast.origin[axis] = ray.origin[axis];
            fast.inverse[axis] = ray.direction[axis] != 0.0f
                ? 1.0f / ray.direction[axis]
                : std::numeric_limits<float>::infinity();
        }
        return fast;
    }

    bool intersectBox(const float origin[3], const float inverse[3],
        const float min[3], const float max[3], float maxDistance)
    {
        float tMin = 0.0f;
        float tMax = maxDistance;

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::isinf(inverse[axis]))
            {
                if (origin[axis] < min[axis] || origin[axis] > max[axis]) return false;
                continue;
            }

            float t0 = (min[axis] - origin[axis]) * inverse[axis];
            float t1 = (max[axis] - origin[axis]) * inverse[axis];
            if (t0 > t1) std::swap(t0, t1);

            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            if (tMin > tMax) return false;
        }

        return true;
    }

    bool worldBoundsHit(const FastRay& ray, const Mesh& mesh, const glm::mat4& worldMatrix, float maxDistance)
    {
        const Bounds bounds = EntityBounds::meshWorldBounds(mesh, worldMatrix);
        const float min[3] = { bounds.min.x, bounds.min.y, bounds.min.z };
        const float max[3] = { bounds.max.x, bounds.max.y, bounds.max.z };
        return intersectBox(ray.origin, ray.inverse, min, max, maxDistance);
    }

    bool intersectTriangle(const glm::vec3& origin, const glm::vec3& direction,
        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t)
    {
        const glm::vec3 edge1 = b - a;
        const glm::vec3 edge2 = c - a;
        const glm::vec3 p = glm::cross(direction, edge2);
        const float determinant = glm::dot(edge1, p);

        if (determinant == 0.0f) return false;

        const float inverse = 1.0f / determinant;
        const glm::vec3 s = origin - a;
        const float u = glm::dot(s, p) * inverse;
        if (u < 0.0f || u > 1.0f) return false;

        const glm::vec3 q = glm::cross(s, edge1);
        const float v = glm::dot(direction, q) * inverse;
        if (v < 0.0f || u + v > 1.0f) return false;

        t = glm::dot(edge2, q) * inverse;
        return t > 0.0f;
    }

    bool intersectLocal(const Ray& ray, const Mesh& mesh, const glm::mat4& worldMatrix, float& distance)
    {
        if (glm::determinant(worldMatrix) == 0.0f) return false;

        const glm::mat4 inverseWorld = glm::inverse(worldMatrix);
        const glm::vec3 origin = glm::vec3(inverseWorld * glm::vec4(ray.origin, 1.0f));
        const glm::vec3 direction = glm::vec3(inverseWorld * glm::vec4(ray.direction, 0.0f));

        const FastRay localRay = makeFastRay(Ray{ origin, direction });
        const float localMin[3] = { mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z };
        const float localMax[3] = { mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z };
        if (!intersectBox(localRay.origin, localRay.inverse, localMin, localMax, distance)) return false;

        bool found = false;
        float t = 0.0f;
        const size_t vertexCount = mesh.vertices.size();

        auto test = [&](size_t i0, size_t i1, size_t i2)
        {
            if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) return;

            if (intersectTriangle(origin, direction, mesh.vertices[i0], mesh.vertices[i1], mesh.vertices[i2], t)
                && t < distance)
            {
                distance = t;
                found = true;
            }
        };

        if (!mesh.indices.empty())
        {
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
            {
                test(mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]);
            }
        }
        else
        {
            for (size_t i = 0; i + 2 < vertexCount; i += 3)
            {
                test(i, i + 1, i + 2);
            }
        }

        return found;
    }
}

Ray RayCast::fromScreen(const glm::vec2& ndc, const glm::mat4& view, const glm::mat4& projection)
{
    const glm::mat4 inverseViewProjection = glm::inverse(projection * view);

    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndc, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndc, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    Ray ray;
    ray.origin = glm::vec3(nearPoint);
    ray.direction = glm::normalize(glm::vec3(farPoint - nearPoint));
    return ray;
}

bool RayCast::intersectMesh(const Ray& ray, const Mesh& mesh, const glm::mat4& worldMatrix, float& distance)
{
    if (mesh.vertices.empty()) return false;
    if (!worldBoundsHit(makeFastRay(ray), mesh, worldMatrix, distance)) return false;

    return intersectLocal(ray, mesh, worldMatrix, distance);
}

bool RayCast::pickMesh(World& world, const Ray& ray, RayHit& hit,
    float maxDistance, const std::function<bool(EntityId)>& accept)
{
    auto& renderers = world.getComponentPool<MeshRenderer>();
    auto& transforms = world.getComponentPool<Transform>();

    const FastRay fastRay = makeFastRay(ray);
    float closest = maxDistance;
    EntityId closestEntity = INVALID_ENTITY;

    for (size_t i = 0; i < renderers.size(); ++i)
    {
        const MeshRenderer& renderer = renderers.components()[i];
        if (!renderer.visible || !renderer.mesh || !renderer.mesh->ready) continue;

        const Mesh& mesh = *renderer.mesh;
        if (mesh.vertices.empty()) continue;

        const EntityId entity = renderers.entities()[i];
        const Transform* transform = transforms.getComponent(entity);
        if (!transform) continue;

        const glm::mat4 worldMatrix = EntityBounds::worldMatrix(world, entity, *transform);

        if (!worldBoundsHit(fastRay, mesh, worldMatrix, closest)) continue;
        if (accept && !accept(entity)) continue;

        if (intersectLocal(ray, mesh, worldMatrix, closest))
        {
            closestEntity = entity;
        }
    }

    if (closestEntity == INVALID_ENTITY) return false;

    hit.entity = closestEntity;
    hit.distance = closest;
    hit.point = ray.origin + ray.direction * closest;
    return true;
}
