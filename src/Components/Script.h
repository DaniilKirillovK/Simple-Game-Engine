#pragma once

#include "Component.h"
#include "Utils/json.hpp"

#include <string>
#include <utility>

// Data-only: which Python behaviour class drives the entity.
// Live Python instances are owned by ScriptSystem, never by the component.
class Script : public Component
{
public:
    Script() = default;

    Script(std::string module, std::string behaviourClass)
        : moduleName(std::move(module)),
        className(std::move(behaviourClass))
    {
    }

	std::string moduleName; // file in scripts/ without .py
	std::string className;  // class derived from engine.Behaviour

    nlohmann::json fields = nlohmann::json::object();
};
