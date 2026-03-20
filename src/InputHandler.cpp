#include "InputHandler.h"
#include "Logger.h"

void InputHandler::update()
{
    mouseDeltaX = 0.0;
    mouseDeltaY = 0.0;

    scrollX = 0.0;
    scrollY = 0.0;
}

void InputHandler::processKeyEvents(const std::vector<KeyEvent>& events)
{
    for (const auto& event : events) 
    {
        currentKeys[event.keyCode] = event.pressed;
        if (event.pressed == true)
        {
            LOG_INPUT("Key pressed");
        }
        else
        {
            LOG_INPUT("Key released");
        }
    }
}

void InputHandler::processMouseButtonEvents(const std::vector<MouseButtonEvent>& events)
{
    for (const auto& event : events) 
    {
        currentMouseButtons[event.button] = event.pressed;
        if (event.pressed == true)
        {
            LOG_INPUT("Mouse button pressed");
        }
        else
        {
            LOG_INPUT("Mouse button released");
        }
    }
}

void InputHandler::processMouseMoveEvents(const std::vector<MouseMoveEvent>& events)
{
    if (!events.empty()) 
    {
        const auto& lastMove = events.back();

        lastMouseX = mouseX;
        lastMouseY = mouseY;

        mouseX = lastMove.x;
        mouseY = lastMove.y;

        mouseDeltaX = mouseX - lastMouseX;
        mouseDeltaY = mouseY - lastMouseY;
    }
}

void InputHandler::processMouseScrollEvents(const std::vector<MouseScrollEvent>& events)
{
    for (const auto& event : events) 
    {
        scrollX += event.xOffset;
        scrollY += event.yOffset;
    }
}

bool InputHandler::isKeyPressed(int key) const
{
    auto it = currentKeys.find(key);
    return it != currentKeys.end() && it->second;
}

bool InputHandler::isMouseButtonPressed(int button) const
{
    auto it = currentMouseButtons.find(button);
    return it != currentMouseButtons.end() && it->second;
}

void InputHandler::clearFrameState()
{
    //currentKeys.clear();
    //currentMouseButtons.clear();
}
