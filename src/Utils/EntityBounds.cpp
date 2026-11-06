#include "Utils/EntityBounds.h"
#include "Components/Collider.h"
#include "Components/Hierarchy.h"

#include <cmath>

#include <glm/gtc/type_ptr.hpp>

namespace
{
    bool isDirtyRoot(World& world, EntityId entity, const Transform& transform)
    {
        if (!transform.worldMatrixDirty) return false;

        const Hierarchy* hierarchy = world.getComponent<Hierarchy>(entity);
        return !hierarchy
            || hierarchy->parent == INVALID_ENTITY
            || hierarchy->parent == static_cast<EntityId>(-1);
    }
}

glm::mat4 EntityBounds::worldMatrix(World& world, EntityId entity, const Transform& transform)
{
    return isDirtyRoot(world, entity, transform) ? transform.getLocalMatrix() : transform.worldMatrix;
}

glm::vec3 EntityBounds::worldPosition(World& world, EntityId entity, const Transform& transform)
{
    return isDirtyRoot(world, entity, transform) ? transform.position : transform.getWorldPosition();
}

void EntityBounds::ensureMeshBounds(const Mesh& mesh)
{
    if (mesh.boundsValid || mesh.vertices.empty()) return;

    glm::vec3 min = mesh.vertices.front();
    glm::vec3 max = mesh.vertices.front();
    for (const glm::vec3& vertex : mesh.vertices)
    {
        min = glm::min(min, vertex);
        max = glm::max(max, vertex);
    }

    mesh.boundsMin = min;
    mesh.boundsMax = max;
    mesh.boundsValid = true;
}

Bounds EntityBounds::meshWorldBounds(const Mesh& mesh, const glm::mat4& worldMatrix)
{
    ensureMeshBounds(mesh);

    const float* m = glm::value_ptr(worldMatrix);
    const float center[3] = {
        (mesh.boundsMin.x + mesh.boundsMax.x) * 0.5f,
        (mesh.boundsMin.y + mesh.boundsMax.y) * 0.5f,
        (mesh.boundsMin.z + mesh.boundsMax.z) * 0.5f,
    };
    const float extent[3] = {
        (mesh.boundsMax.x - mesh.boundsMin.x) * 0.5f,
        (mesh.boundsMax.y - mesh.boundsMin.y) * 0.5f,
        (mesh.boundsMax.z - mesh.boundsMin.z) * 0.5f,
    };

    Bounds bounds;
    for (int row = 0; row < 3; ++row)
    {
        float worldCenter = m[12 + row];
        float worldExtent = 0.0f;
        for (int column = 0; column < 3; ++column)
        {
            const float value = m[column * 4 + row];
            worldCenter += value * center[column];
            worldExtent += std::abs(value) * extent[column];
        }
        bounds.min[row] = worldCenter - worldExtent;
        bounds.max[row] = worldCenter + worldExtent;
    }
    return bounds;
}

bool EntityBounds::compute(World& world, EntityId entity, Bounds& out)
{
    const Transform* transform = world.getComponent<Transform>(entity);
    if (!transform) return false;

    if (const Collider* collider = world.getComponent<Collider>(entity))
    {
        const glm::vec3 center = worldPosition(world, entity, *transform) + collider->offset;
        const glm::vec3 half = collider->type == ColliderType::Box
            ? glm::abs(collider->halfSize)
            : glm::vec3(std::abs(collider->radius));
        out.min = center - half;
        out.max = center + half;
        return true;
    }

    const MeshRenderer* renderer = world.getComponent<MeshRenderer>(entity);
    if (!renderer || !renderer->mesh || !renderer->mesh->ready || renderer->mesh->vertices.empty()) return false;

    out = meshWorldBounds(*renderer->mesh, worldMatrix(world, entity, *transform));
    return true;
}

bool EntityBounds::overlap(const Bounds& a, const Bounds& b, float margin)
{
    return a.min.x <= b.max.x + margin && a.max.x >= b.min.x - margin
        && a.min.y <= b.max.y + margin && a.max.y >= b.min.y - margin
        && a.min.z <= b.max.z + margin && a.max.z >= b.min.z - margin;
}
