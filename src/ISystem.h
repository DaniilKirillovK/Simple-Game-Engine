#pragma once
#include "JobSystem/JobSystem.h"

class World;

class ISystem
{
public:
	virtual void update(World& world, JobSystem* jobs, float deltaTime) = 0;
	virtual void setEnabled(bool isEnabled) = 0;
};