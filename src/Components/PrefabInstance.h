#pragma once
#include "Component.h"
#include <string>

// Marks an entity spawned from a template, so the editor can apply or revert its
// script fields against assets/prefabs/<prefab>.prefab.json
class PrefabInstance : public Component
{
public:
	PrefabInstance() = default;
	explicit PrefabInstance(std::string prefabName) : prefab(std::move(prefabName)) {}

	std::string prefab;
};
