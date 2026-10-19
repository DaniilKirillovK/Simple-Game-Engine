#pragma once
#include "Entity.h"

class ScriptBehaviour
{
public:
	virtual ~ScriptBehaviour() = default;

	virtual void onStart() {}
	virtual void onUpdate(float deltaTime) {}
	virtual void onDestroy() {}

	EntityId entityId = INVALID_ENTITY;
};
