#pragma once
class World;

class ISystem
{
public:
	virtual void update(World& world, float deltaTime) = 0;
};