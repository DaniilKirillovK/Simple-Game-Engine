#pragma once
#include "Component.h"
#include "Material.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

    std::string path;

    bool ready = true;

    // OpenGL buffers: owned by the render adapter, set when the mesh is first drawn
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;

    // The render adapter registers this to free the GPU buffers of a dying mesh.
    // A plain function pointer: meshes in static resource caches die after the adapter
    using GpuReleaseHook = void (*)(void* context, Mesh* mesh);
    static inline GpuReleaseHook gpuReleaseHook = nullptr;
    static inline void* gpuReleaseContext = nullptr;

    Mesh() = default;
    Mesh(const Mesh&) = default;
    Mesh(Mesh&&) = default;
    Mesh& operator=(const Mesh&) = default;
    Mesh& operator=(Mesh&&) = default;

    ~Mesh()
    {
        // Only drawn meshes have buffers, and those are drawn and destroyed on the main thread
        if (VAO != 0 && gpuReleaseHook)
        {
            gpuReleaseHook(gpuReleaseContext, this);
        }
    }
};

class MeshRenderer : public Component
{
public:
    MeshRenderer() = default;
    MeshRenderer(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
        : mesh(std::move(mesh)), material(std::move(material)) {}

    // Shared: prefab instances use one mesh and one material, the last owner frees them.
    // Meshes from the asset cache alias their Resource and keep it alive
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool visible = true;
    int renderLayer = 0;
};