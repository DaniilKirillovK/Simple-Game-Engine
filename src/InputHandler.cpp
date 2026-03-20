#include "InputHandler.h"

void InputHandler::update()
{
}

bool InputHandler::isKeyPressed(int key) const
{
	auto it = keyStates.find(key);
	return it != keyStates.end() && it->second;
}

bool InputHandler::isMouseButtonPressed(int button) const
{
	auto it = mouseButtonStates.find(button);
	return it != mouseButtonStates.end() && it->second;
}

void InputHandler::setKeyState(int key, bool pressed)
{
	keyStates[key] = pressed;
}

void InputHandler::setMouseButtonState(int button, bool pressed)
{
	mouseButtonStates[button] = pressed;
}

void InputHandler::setMousePosition(int x, int y)
{
	mouseX = x;
	mouseY = y;
}
