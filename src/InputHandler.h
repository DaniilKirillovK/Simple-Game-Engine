#pragma once
#include <cstdint>
#include <unordered_map>

struct KeyEvent
{
    int keyCode;
    bool pressed;
};

struct MouseButtonEvent
{
    int button;
    bool pressed;
};

struct MouseMoveEvent
{
    int x, y;
};

struct MouseScrollEvent 
{
    double xOffset, yOffset;
};

class InputHandler 
{
public:
    InputHandler() = default;

    void update();

    void processKeyEvents(const std::vector<KeyEvent>& events);
    void processMouseButtonEvents(const std::vector<MouseButtonEvent>& events);
    void processMouseMoveEvents(const std::vector<MouseMoveEvent>& events);
    void processMouseScrollEvents(const std::vector<MouseScrollEvent>& events);

    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;

    double getMouseX() const { return mouseX; }
    double getMouseY() const { return mouseY; }
    double getMouseDeltaX() const { return mouseDeltaX; }
    double getMouseDeltaY() const { return mouseDeltaY; }

    double getMouseScrollX() const { return scrollX; }
    double getMouseScrollY() const { return scrollY; }

    void clearFrameState();
private:
    std::unordered_map<int, bool> currentKeys;
    std::unordered_map<int, bool> currentMouseButtons;

    double mouseX = 0.0;
    double mouseY = 0.0;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    double mouseDeltaX = 0.0;
    double mouseDeltaY = 0.0;

    double scrollX = 0.0;
    double scrollY = 0.0;
};