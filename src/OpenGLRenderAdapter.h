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

private:
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    bool initGLFW(int width, int height);
    bool initGLAD();
    void setupOpenGLState();

    void onFramebufferResize(int width, int height);

    void setupSquare();
    void drawSquare();

    void updateProjection();

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
};