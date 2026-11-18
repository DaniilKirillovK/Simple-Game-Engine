#pragma once
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "IRenderAdapter.h"
#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <vector>

struct DebugVertex;
struct GLFWwindow;
struct ImDrawData;
struct Mesh;
struct TextureData;
class Light;
class Material;

class GpuRenderer
{
public:
    explicit GpuRenderer(bool vulkan, bool verify);
    ~GpuRenderer();

    GLFWwindow* initialize(int width, int height);
    void run(const std::function<void()>& frame);

    uint32_t compileShader(const std::string& source, ShaderType type);
    uint32_t uploadTexture(const TextureData& data);
    void destroyTexture(uint32_t id);
    void releaseMesh(const Mesh* mesh);
    void resizeScene(int width, int height);
    uint32_t sceneTexture() const;

    void initGui();
    void drawGui(const ImDrawData& data);

    void drawLines(const std::vector<DebugVertex>& vertices, const glm::mat4& view, const glm::mat4& projection);
    void drawMesh(const Mesh& mesh, const ShaderProgram& program, const Material& material,
        const std::vector<Light*>& lights, const glm::mat4& model, const glm::mat4& view,
        const glm::mat4& projection, const glm::mat4& normal);

    // Instanced / indirect path: one draw call for all transforms of one mesh.
    void drawMeshInstanced(const Mesh& mesh, const ShaderProgram& program, const Material& material,
        const std::vector<Light*>& lights, const glm::mat4& view, const glm::mat4& projection,
        const std::vector<InstanceData>& instances, bool indirect);

    RenderStats stats() const;

private:
    struct State;
    std::unique_ptr<State> state;
};
