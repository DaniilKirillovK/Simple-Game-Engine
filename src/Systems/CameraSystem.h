#pragma once
#include "ISystem.h"
#include "InputHandler.h"

class CameraSystem : public ISystem
{
public:
	CameraSystem() {}

	virtual void update(World& world, float deltaTime) override;
	virtual void setEnabled(bool isEnabled) override;

private:
	bool m_isEnabled = true;
};