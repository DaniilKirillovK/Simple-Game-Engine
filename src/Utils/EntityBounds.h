#pragma once
#include "World.h"
#include "Components/MeshRenderer.h"
#include "Components/Transform.h"
#include <glm/glm.hpp>

struct Bounds
{
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
};

class EntityBounds
{
public:
    static glm::mat4 worldMatrix(World& world, EntityId entity, const Transform& transform);
    static glm::vec3 worldPosition(World& world, EntityId entity, const Transform& transform);

    static void ensureMeshBounds(const Mesh& mesh);
    static Bounds meshWorldBounds(const Mesh& mesh, const glm::mat4& worldMatrix);

    static bool compute(World& world, EntityId entity, Bounds& out);
    static bool overlap(const Bounds& a, const Bounds& b, float margin = 0.0f);
};
