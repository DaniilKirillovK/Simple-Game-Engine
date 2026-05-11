#include "InputHandler.h"
#include "Logger.h"

void InputHandler::update()
{
    m_previousKeys = m_currentKeys;
    m_previousMouseButtons = m_currentMouseButtons;

    mouseDeltaX = 0.0;
    mouseDeltaY = 0.0;

    scrollX = 0.0;
    scrollY = 0.0;
}

void InputHandler::processKeyEvents(const std::vector<KeyEvent>& events)
{
    for (const auto& event : events) 
    {
        m_currentKeys[event.keyCode] = event.pressed;
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
        m_currentMouseButtons[event.button] = event.pressed;
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

bool InputHandler::isKeyPressed(KeyCode key) const
{
    auto it = m_currentKeys.find(key);
    return it != m_currentKeys.end() && it->second;
}

bool InputHandler::isKeyJustPressed(KeyCode key) const
{
    bool current = isKeyPressed(key);
    auto it = m_previousKeys.find(key);
    bool previous = (it != m_previousKeys.end()) ? it->second : false;
    return current && !previous;
}

bool InputHandler::isKeyJustReleased(KeyCode key) const
{
    bool current = isKeyPressed(key);
    auto it = m_previousKeys.find(key);
    bool previous = (it != m_previousKeys.end()) ? it->second : false;
    return !current && previous;
}

bool InputHandler::isMouseButtonPressed(int button) const
{
    auto it = m_currentMouseButtons.find(button);
    return it != m_currentMouseButtons.end() && it->second;
}

bool InputHandler::isMouseButtonJustPressed(int button) const
{
    bool current = isMouseButtonPressed(button);
    auto it = m_previousMouseButtons.find(button);
    bool previous = (it != m_previousMouseButtons.end()) ? it->second : false;
    return current && !previous;
}

bool InputHandler::isMouseButtonJustReleased(int button) const
{
    bool current = isMouseButtonPressed(button);
    auto it = m_previousMouseButtons.find(button);
    bool previous = (it != m_previousMouseButtons.end()) ? it->second : false;
    return !current && previous;
}

void InputHandler::bindAction(const std::string& actionName, KeyCode keyCode)
{
    m_actionMap[actionName] = keyCode;
    LOG_INFO("Action bound: " + actionName + " -> key " + std::to_string(static_cast<int>(keyCode)));
}

void InputHandler::unbindAction(const std::string& actionName)
{
    auto it = m_actionMap.find(actionName);
    if (it != m_actionMap.end()) 
    {
        m_actionMap.erase(it);
        LOG_INFO("Action unbound: " + actionName);
    }
}

bool InputHandler::isActionActive(const std::string& actionName) const
{
    auto it = m_actionMap.find(actionName);
    if (it != m_actionMap.end()) 
    {
        return isKeyPressed(it->second);
    }
    return false;
}

KeyCode InputHandler::getActionKey(const std::string& actionName) const
{
    auto it = m_actionMap.find(actionName);
    if (it != m_actionMap.end()) 
    {
        return it->second;
    }
    return KeyCode::KEY_UNKNOWN;
}

bool InputHandler::isActionJustPressed(const std::string& actionName) const
{
    auto it = m_actionMap.find(actionName);
    if (it != m_actionMap.end()) 
    {
        return isKeyJustPressed(it->second);
    }
    return false;
}

bool InputHandler::isActionJustReleased(const std::string& actionName) const
{
    auto it = m_actionMap.find(actionName);
    if (it != m_actionMap.end()) 
    {
        return isKeyJustReleased(it->second);
    }
    return false;
}

void InputHandler::clearFrameState()
{
    //currentKeys.clear();
    //currentMouseButtons.clear();
}
