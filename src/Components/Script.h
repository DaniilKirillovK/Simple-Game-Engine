#pragma once
#include "Component.h"
#include <string>

// Data-only: which Python behaviour class drives the entity.
// Live Python instances are owned by ScriptSystem, never by the component.
class Script : public Component
{
public:
	Script() {};
	Script(std::string moduleName, std::string className)
		: moduleName(moduleName), className(className) {}

	std::string moduleName; // file in scripts/ without .py
	std::string className;  // class derived from engine.Behaviour
};
