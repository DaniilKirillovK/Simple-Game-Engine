#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include "InputHandler.h"
#include "Resources/ShaderLoader/ShaderProgram.h"

class Material;
class Mesh;
class Light;
struct TextureData;
struct ShaderProgram;
class World;

struct Color 
{
    uint8_t r, g, b, a;
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {
    }
};


class IRenderAdapter
{
public:
    virtual ~IRenderAdapter() = default;
    virtual bool initialize(int width, int height) = 0;
    virtual void render(World* world) = 0;
    virtual void shutdown() = 0;
    virtual bool shouldClose() const = 0;
    virtual void compileShaders() = 0;
    virtual bool checkShaderCompileErrors(unsigned int shader, const std::string& type) = 0;
    virtual void pollEvents() = 0;

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

    virtual void loadAssetIcons() = 0;

    // Shader compile and linkage
    virtual unsigned int compileShaderSource(const std::string& source, ShaderType type) = 0;
    virtual unsigned int linkShaderProgram(unsigned int vertexShader, unsigned int fragmentShader) = 0;
    virtual void deleteShaderObject(unsigned int shader) = 0;
    virtual void deleteShaderProgram(unsigned int program) = 0;

    // Callbacks
    virtual void setOnToggleDebugCallback(std::function<void(bool)> callback) = 0;
    virtual void setOnPlayCallback(std::function<void()> callback) = 0;
    virtual void setOnStopCallback(std::function<void()> callback) = 0;
};