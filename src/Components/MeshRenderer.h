#pragma once
#include "Component.h"
#include "Material.h"
#include <glm/glm.hpp>

enum class MeshType 
{
    Cube,
    Sphere,
    Plane,
    Custom
};

struct Mesh 
{
    MeshType type = MeshType::Cube;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<unsigned int> indices;

    // OpenGL buffers
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;
};

class MeshRenderer : public Component
{
public:
    MeshRenderer() : mesh(nullptr), material(nullptr) {};
    MeshRenderer(Mesh* mesh, Material* material)
        : mesh(mesh), material(material) {}

    Mesh* mesh;
    Material* material;
    bool visible = true;
    int renderLayer = 0;
};