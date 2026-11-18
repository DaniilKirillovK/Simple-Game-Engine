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

    // Verification support: pass 0 renders with the current settings and writes
    // scene-<api>.ppm, pass 1 (set by the adapter for the equivalence check) renders
    // with instancing disabled and writes scene-<api>-off.ppm. The passes are compared
    // in memory while the pass-1 frame is on the CPU; the PPM files are artifacts only.
    void setVerifyPass(int pass);

    // Returns the in-memory comparison verdict recorded during pass 1.
    bool compareVerifyFrames() const;

private:
    struct State;
    std::unique_ptr<State> state;
};
