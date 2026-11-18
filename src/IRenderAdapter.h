#pragma once
#include <string>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "InputHandler.h"
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "Entity.h"

class Material;
struct Mesh;
class Light;
struct TextureData;
struct ShaderProgram;
class World;
class PrefabLibrary;

struct Color 
{
    uint8_t r, g, b, a;
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {
    }
};

// Per-instance data for the instanced / indirect path.
// Layout matches 2 x float4x4 in the shader: model matrix + normal matrix.
struct InstanceData
{
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 normal = glm::mat4(1.0f);
};

// Draw statistics of the last rendered frame, for the instancing demo.
struct RenderStats
{
    uint32_t regularDraws = 0;     // per-object draw calls (non-instanced)
    uint32_t instancedDraws = 0;   // instanced draw calls (direct)
    uint32_t indirectDraws = 0;    // instanced draws issued via an indirect args buffer
    uint32_t batchedInstances = 0; // instances submitted through the batched path
    uint32_t batches = 0;          // number of instanced batches flushed
};


class IRenderAdapter
{
public:
    virtual ~IRenderAdapter() = default;
    virtual bool initialize(int width, int height) = 0;
    virtual void render(World* world) = 0;
    virtual void shutdown() = 0;
    virtual bool shouldClose() const = 0;
    virtual void clearEvents() = 0;
    virtual void runFrames(const std::function<void()>& frame) = 0;

    virtual void onKey(int key, int action) = 0;
    virtual void onMouseButton(int button, int action) = 0;
    virtual void onMouseMove(double x, double y) = 0;
    virtual void onMouseScroll(double xoffset, double yoffset) = 0;

    virtual std::vector<KeyEvent> getKeyEvents() = 0;
    virtual std::vector<MouseButtonEvent> getMouseButtonEvents() = 0;
    virtual std::vector<MouseMoveEvent> getMouseMoveEvents() = 0;
    virtual std::vector<MouseScrollEvent> getMouseScrollEvents() = 0;

    virtual uint32_t createTexture(const TextureData& data) = 0;
    virtual void destroyTexture(uint32_t handle) = 0;

    // ImGui
    virtual void initImGui() = 0;
    virtual void beginImGuiFrame() = 0;
    virtual void endImGuiFrame() = 0;
    virtual void shutdownImGui() = 0;

    // 3D
    virtual void setModelMatrix(const float* matrix) = 0;
    virtual void setViewMatrix(const float* matrix) = 0;
    virtual void setProjectionMatrix(const float* matrix) = 0;
    virtual void setNormalMatrix(const float* matrix) = 0;

    virtual float getAspectRatio() = 0;

    virtual void setShaderProgram(const ShaderProgram* shaderProgram) = 0;
    virtual void setMaterial(const Material* material) = 0;
    virtual void setLights(const std::vector<Light*>& lights) = 0;

    // Debug
    virtual void beginDebugDraw() = 0;
    virtual void endDebugDraw() = 0;
    virtual void drawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) = 0;
    virtual void drawDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color) = 0;
    virtual void drawDebugSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments = 16) = 0;

    virtual void drawMesh(const Mesh* mesh) = 0;

    // Draws one mesh with many transforms in a single draw call.
    // When 'indirect' is true the draw arguments (index/vertex count, instance count)
    // are written to a GPU indirect-arguments buffer and submitted via drawIndexedIndirect /
    // drawIndirect; otherwise a plain instanced draw is used.
    virtual void drawMeshInstanced(const Mesh* mesh, const std::vector<InstanceData>& instances, bool indirect) = 0;

    // Instancing feature toggles (renderer submits batched draws only when enabled).
    virtual void setInstancingEnabled(bool enabled) = 0;
    virtual void setIndirectDrawEnabled(bool enabled) = 0;
    virtual bool isInstancingEnabled() const = 0;
    virtual bool isIndirectDrawEnabled() const = 0;
    virtual RenderStats getRenderStats() const = 0;

    // Loads a scene file into the live world (used by the editor and the script console).
    virtual bool loadScene(const std::string& path) = 0;

    // Tracks the path of the currently loaded scene: the editor autosave writes back
    // to this path, so it must always match the file that was actually loaded.
    virtual void setScenePath(const std::string& path) = 0;

    virtual void loadAssetIcons() = 0;

    // Shader compile and linkage
    virtual unsigned int compileShaderSource(const std::string& source, ShaderType type) = 0;

    // Callbacks
    virtual void setOnToggleDebugCallback(std::function<void(bool)> callback) = 0;
    virtual void setOnPlayCallback(std::function<void()> callback) = 0;
    virtual void setOnStopCallback(std::function<void()> callback) = 0;
    virtual void setOnCreateCubeCallback(std::function<void()> callback) = 0;
    virtual void setOnCreateSphereCallback(std::function<void()> callback) = 0;
    virtual void setOnCreateEmptyCallback(std::function<void()> callback) = 0;
    virtual void setOnCreateFromAssetCallback(std::function<void(std::shared_ptr<Mesh>, const std::string&)> callback) = 0;

    virtual void setSelectedEntity(EntityId entity) = 0;

    // Editor: prefab list, instantiate into the scene, save selection as a template
    virtual void setPrefabLibrary(PrefabLibrary* library) = 0;
};

// Process-wide access point to the active render adapter, used by the engine
// console (Python `engine` module) and by debug commands.
namespace RenderApi
{
    void setAdapter(IRenderAdapter* adapter);
    IRenderAdapter* getAdapter();
}