#pragma once
#include "ISystem.h"
#include "InputHandler.h"

class CameraSystem : public ISystem
{
public:
	CameraSystem(InputHandler* inputHandler) : inputHandler(inputHandler) {}

	virtual void update(World& world, float deltaTime) override;

private:
	InputHandler* inputHandler;
};