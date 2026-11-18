#pragma once

// Shared scripting helpers: entity handles and the active play session.
// These live in a header so both ScriptBindings.cpp (the Python API) and
// ScriptSystem.cpp (state capture/apply) can resolve entities and components.

#include "Entity.h"
#include "World.h"
#include "Scripting/ScriptEngine.h"
#include "Systems/ScriptSystem.h"
#include "Components/Tag.h"

#include <cstdint>
#include <stdexcept>
#include <string>

struct EntityHandle
{
	EntityId id = INVALID_ENTITY;
	std::uint64_t sessionId = 0;

	EntityHandle() = default;

	explicit EntityHandle(EntityId entityId) : id(entityId)
	{
		ScriptSystem* system = ScriptEngine::getInstance().getActiveSystem();
		if (system)
		{
			sessionId = system->getSessionId();
		}
	}
};

// The active play session. Throws a Python-visible error outside Play mode,
// which is exactly what the engine.* queries rely on.
inline ScriptSystem& activeSystem()
{
	ScriptSystem* system = ScriptEngine::getInstance().getActiveSystem();
	if (!system) throw std::runtime_error("Engine API is available only in play mode");
	return *system;
}

inline World& activeWorld()
{
	return activeSystem().getWorld();
}

// A handle is valid only while its play session is alive and the entity still
// exists. This is what prevents dangling references a saved handle could hold.
inline bool isValid(const EntityHandle& entity)
{
	ScriptSystem* system = ScriptEngine::getInstance().getActiveSystem();
	return system &&
		entity.sessionId == system->getSessionId() &&
		system->getWorld().isValidEntity(entity.id);
}

inline void requireValid(const EntityHandle& entity)
{
	if (!isValid(entity))
	{
		throw std::runtime_error(
			"Entity " + std::to_string(entity.id) +
			" is destroyed or belongs to an expired Play session");
	}
}

// Returns a component reference or throws; used by the Entity.* properties.
template<typename T>
inline T& requireComponent(const EntityHandle& entity, const char* componentName)
{
	requireValid(entity);
	T* component = activeWorld().getComponent<T>(entity.id);
	if (!component)
	{
		throw std::runtime_error("Entity " + std::to_string(entity.id) + " has no " + componentName);
	}
	return *component;
}

inline std::string entityName(const EntityHandle& entity)
{
	requireValid(entity);
	Tag* tag = activeWorld().getComponent<Tag>(entity.id);
	return tag ? tag->name : std::string();
}

inline void setEntityName(const EntityHandle& entity, const std::string& name)
{
	requireValid(entity);
	World& world = activeWorld();
	if (Tag* tag = world.getComponent<Tag>(entity.id))
	{
		tag->name = name;
	}
	else
	{
		world.getComponentPool<Tag>().addComponent(entity.id, Tag{ name });
	}
}
