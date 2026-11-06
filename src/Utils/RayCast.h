#pragma once
#include "World.h"
#include "Components/MeshRenderer.h"
#include <glm/glm.hpp>
#include <functional>
#include <limits>

struct Ray
{
    glm::vec3 origin = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
};

struct RayHit
{
    EntityId entity = INVALID_ENTITY;
    float distance = 0.0f;
    glm::vec3 point = glm::vec3(0.0f);
};

class RayCast
{
public:
    static Ray fromScreen(const glm::vec2& ndc, const glm::mat4& view, const glm::mat4& projection);
    static bool intersectMesh(const Ray& ray, const Mesh& mesh, const glm::mat4& worldMatrix, float& distance);
    static bool pickMesh(World& world, const Ray& ray, RayHit& hit,
        float maxDistance = std::numeric_limits<float>::max(),
        const std::function<bool(EntityId)>& accept = {});
};
