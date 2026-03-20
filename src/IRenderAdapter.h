#pragma once
#include <string>
#include <cstdint>
#include "InputHandler.h"

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
};