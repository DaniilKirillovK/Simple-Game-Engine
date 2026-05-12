#pragma once
#include "IRenderAdapter.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include "libs/imgui/imgui.h"
#include "Entity.h"

class Transform;

struct DebugVertex
{
    glm::vec3 position;
    glm::vec4 color;
};

struct ShaderProgram;

class OpenGLRenderAdapter : public IRenderAdapter
{
public:
    OpenGLRenderAdapter();
    virtual ~OpenGLRenderAdapter();

    virtual bool initialize(int width, int height) override;
    virtual void render(World* world) override;
    virtual void shutdown() override;
    virtual bool shouldClose() const override;
    virtual void compileShaders() override;
    virtual bool checkShaderCompileErrors(unsigned int shader, const std::string& type) override;
    virtual void pollEvents() override;

    virtual std::vector<KeyEvent> getKeyEvents() override { return keyEvents; }
    virtual std::vector<MouseButtonEvent> getMouseButtonEvents() override { return mouseButtonEvents; }
    virtual std::vector<MouseMoveEvent> getMouseMoveEvents() override { return mouseMoveEvents; }
    virtual std::vector<MouseScrollEvent> getMouseScrollEvents() override { return mouseScrollEvents; }

    virtual unsigned int compileShaderSource(const std::string& source, ShaderType type) override;
    virtual unsigned int linkShaderProgram(unsigned int vertexShader, unsigned int fragmentShader) override;
    virtual void deleteShaderObject(unsigned int shader) override;
    virtual void deleteShaderProgram(unsigned int program) override;

    virtual void initImGui() override;
    virtual void beginImGuiFrame() override;
    virtual void endImGuiFrame() override;
    virtual void shutdownImGui() override;

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    virtual void onKey(int key, int action) override;
    virtual void onMouseButton(int button, int action) override;
    virtual void onMouseMove(double x, double y) override;
    virtual void onMouseScroll(double xoffset, double yoffset) override;

    virtual uint32_t createTexture(const TextureData& data) override;
    virtual void destroyTexture(uint32_t handle) override;

    virtual void setModelMatrix(const float* matrix) override;
    virtual void setViewMatrix(const float* matrix) override;
    virtual void setProjectionMatrix(const float* matrix) override;
    virtual void setNormalMatrix(const float* matrix) override;
    virtual float getAspectRatio() override;

    virtual void setShaderProgram(const ShaderProgram* shaderProgram) override;
    virtual void setMaterial(const Material* material) override;
    virtual void setLights(const std::vector<Light*>& lights) override;

    virtual void beginDebugDraw() override;
    virtual void endDebugDraw() override;
    virtual void drawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) override;
    virtual void drawDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color) override;
    virtual void drawDebugSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments = 16) override;

    virtual void drawMesh(const Mesh* mesh) override;

    void createRenderTexture(int width, int height);
    void beginRenderToTexture();
    void endRenderToTexture();
    unsigned int getRenderTexture() const { return m_renderTexture; }
    void resizeRenderTexture(int width, int height);

    void renderUI(World* world);
    void renderUIViewport(World* world);
    void renderSceneHierarchy(World* world);
    void renderInspector(World* world);
    void renderStatistics(World* world);
    void renderAboutWindow();
    void renderTransformEditor(Transform& transform);
    void renderToolbar(ImVec2 position, ImVec2 size);
    static void SizeCallback(ImGuiSizeCallbackData* data);

    bool initGLFW(int width, int height);
    bool initGLAD();
    void setupOpenGLState();

    void onFramebufferResize(int width, int height);
    
    void updateProjectionMatrix();

    void createMeshVAO(Mesh* mesh);

    void setupDebugBuffers();
    void flushDebugDraw();
    void createDebugShader();

    // Key Events
    std::vector<KeyEvent> keyEvents;
    std::vector<MouseButtonEvent> mouseButtonEvents;
    std::vector<MouseMoveEvent> mouseMoveEvents;
    std::vector<MouseScrollEvent> mouseScrollEvents;

    glm::mat4 currentModelMatrix = glm::mat4(1.0f);
    glm::mat4 currentViewMatrix = glm::mat4(1.0f);
    glm::mat4 currentProjectionMatrix = glm::mat4(1.0f);
    glm::mat4 currentNormalMatrix = glm::mat4(1.0f);

    const Material* currentMaterial = nullptr;
    std::vector<Light*> currentLights;

    // ShaderLoader & buffers
    const ShaderProgram* m_shaderProgram;

    std::unordered_map<Mesh*, GLuint> meshVAOs;
    std::unordered_map<Mesh*, GLuint> meshVBOs;
    std::unordered_map<Mesh*, GLuint> meshEBOs;

    GLFWwindow* m_window;
    int m_width;
    int m_height;

    int m_fpsCounter = 0;
    float m_fpsAccumulator = 0.0f;
    int m_currentFPS = 0;

    unsigned int m_fbo = 0;
    unsigned int m_renderTexture = 0;
    unsigned int m_depthBuffer = 0;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
    EntityId m_selectedEntity = -1;

    int m_gizmoOperation = 0;
    bool m_gizmoLocalSpace = false;

    // Debug
    unsigned int m_debugVAO = 0;
    unsigned int m_debugVBO = 0;
    unsigned int m_debugShaderProgram = 0;
    std::vector<DebugVertex> m_debugVertices;

    ImVec2 m_viewportSize = ImVec2(0, 0);
    bool m_viewportHovered = false;

    static constexpr unsigned int DEFAULT_WIDTH = 1920;
    static constexpr unsigned int DEFAULT_HEIGHT = 1080;
};
