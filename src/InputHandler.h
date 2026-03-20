#pragma once
#include <cstdint>
#include <unordered_map>

class InputHandler 
{
public:
    void update();
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    void setKeyState(int key, bool pressed);
    void setMouseButtonState(int button, bool pressed);
    void setMousePosition(int x, int y);
    int getMouseX() const { return mouseX; }
    int getMouseY() const { return mouseY; }
private:
    std::unordered_map<int, bool> keyStates;
    std::unordered_map<int, bool> mouseButtonStates;
    int mouseX = 0, mouseY = 0;
};