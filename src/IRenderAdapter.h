#pragma once
#include <string>
#include <cstdint>
#include "InputHandler.h"

class Material;
class Mesh;
class Light;

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
    virtual void render() = 0;
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

    // 3D
    virtual void setModelMatrix(const float* matrix) = 0;
    virtual void setViewMatrix(const float* matrix) = 0;
    virtual void setProjectionMatrix(const float* matrix) = 0;
    virtual void setNormalMatrix(const float* matrix) = 0;

    virtual float getAspectRatio() = 0;

    virtual void setMaterial(const Material* material) = 0;
    virtual void setLights(const std::vector<Light*>& lights) = 0;

    virtual void drawMesh(const Mesh* mesh) = 0;
};