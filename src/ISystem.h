#pragma once
class World;

class ISystem
{
public:
	virtual void update(World& world, float deltaTime) = 0;

	virtual void setEnabled(bool isEnabled) = 0;
};