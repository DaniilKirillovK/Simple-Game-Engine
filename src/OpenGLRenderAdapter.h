#pragma once
#include "IRenderAdapter.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

class OpenGLRenderAdapter : public IRenderAdapter
{
public:
    OpenGLRenderAdapter();
    virtual ~OpenGLRenderAdapter();

    virtual bool initialize(int width, int height) override;
    virtual void render() override;
    virtual void shutdown() override;
    virtual bool shouldClose() const override;
    virtual void compileShaders() override;
    virtual bool checkShaderCompileErrors(unsigned int shader, const std::string& type) override;
    virtual void pollEvents() override;

    virtual std::vector<KeyEvent> getKeyEvents() override { return keyEvents; }
    virtual std::vector<MouseButtonEvent> getMouseButtonEvents() override { return mouseButtonEvents; }
    virtual std::vector<MouseMoveEvent> getMouseMoveEvents() override { return mouseMoveEvents; }
    virtual std::vector<MouseScrollEvent> getMouseScrollEvents() override { return mouseScrollEvents; }

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

    bool initGLFW(int width, int height);
    bool initGLAD();
    void setupOpenGLState();

    void onFramebufferResize(int width, int height);

    void setupSquare();
    void drawSquare();

    void updateProjection();

    // Key Events
    std::vector<KeyEvent> keyEvents;
    std::vector<MouseButtonEvent> mouseButtonEvents;
    std::vector<MouseMoveEvent> mouseMoveEvents;
    std::vector<MouseScrollEvent> mouseScrollEvents;

    // Projection
    unsigned int m_projectionLoc;
    float m_aspectRatio;

    // Shaders & buffers
    unsigned int m_shaderProgram;
    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;

    GLFWwindow* m_window;
    int m_width;
    int m_height;

    static constexpr unsigned int DEFAULT_WIDTH = 800;
    static constexpr unsigned int DEFAULT_HEIGHT = 600;
};