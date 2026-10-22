#include "ScriptSystem.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptBehaviour.h"
#include "Components/Script.h"
#include "World.h"
#include "Logger.h"
#include "Scripting/ScriptFields.h"

#include <algorithm>

#include "tracy/Tracy.hpp"

namespace py = pybind11;

ScriptSystem::ScriptSystem(SpawnFunction spawnFunction)
	: m_spawnFunction(std::move(spawnFunction))
{
}

ScriptSystem::~ScriptSystem()
{
	if (ScriptEngine::getInstance().isInitialized())
	{
		stop();
		return;
	}

	// Interpreter is already gone: leak the handles instead of touching dead Python state
	for (auto& [entity, behaviour] : m_behaviours)
	{
		behaviour.release();
	}
}

void ScriptSystem::update(World& world, float deltaTime)
{
	ZoneScopedN("ScriptSystem::update");

	ScriptEngine::getInstance().pollFileChanges(deltaTime, !m_running);

	if (!m_running || !m_isEnabled) return;

	m_world = &world;

	std::vector<EntityId> entities;
	entities.reserve(m_behaviours.size());
	for (auto& [entity, behaviour] : m_behaviours)
	{
		entities.push_back(entity);
	}

	for (EntityId entity : entities)
	{
		auto it = m_behaviours.find(entity);
		if (it == m_behaviours.end()) continue;

		if (!world.isValidEntity(entity))
		{
			m_behaviours.erase(it);
			continue;
		}

		py::object behaviour = it->second;
		try
		{
			behaviour.cast<ScriptBehaviour*>()->onUpdate(deltaTime);
		}
		catch (const std::exception& e)
		{
			ScriptEngine::logPythonError("on_update of entity " + std::to_string(entity)
				+ " failed, behaviour disabled until next Play", e);
			m_behaviours.erase(entity);
		}
	}

	flushDestroyQueue();
}

void ScriptSystem::setEnabled(bool isEnabled)
{
	m_isEnabled = isEnabled;
}

void ScriptSystem::start(World& world)
{
	ZoneScopedN("ScriptSystem::start");

	stop();

	ScriptEngine& engine = ScriptEngine::getInstance();
	if (!engine.isInitialized()) return;

	static std::uint64_t nextSessionId = 0;

	m_sessionId = ++nextSessionId;
	m_world = &world;
	m_running = true;

	engine.setActiveSystem(this);

	// Copy: behaviours may spawn entities and grow the pool while starting
	std::vector<std::pair<EntityId, Script>> scripts;
	auto& scriptPool = world.getComponentPool<Script>();
	for (size_t i = 0; i < scriptPool.size(); ++i)
	{
		scripts.emplace_back(scriptPool.entities()[i], scriptPool.components()[i]);
	}

	for (const auto& [entity, script] : scripts)
	{
		createBehaviour(entity, script);
	}

	// on_start after every behaviour exists, so scripts can find each other
	for (const auto& [entity, script] : scripts)
	{
		auto it = m_behaviours.find(entity);
		if (it == m_behaviours.end()) continue;

		py::object behaviour = it->second;
		try
		{
			behaviour.cast<ScriptBehaviour*>()->onStart();
		}
		catch (const std::exception& e)
		{
			ScriptEngine::logPythonError("on_start of entity " + std::to_string(entity) + " failed", e);
			m_behaviours.erase(entity);
		}
	}

	flushDestroyQueue();

	LOG_INFO("Scripts started: " + std::to_string(m_behaviours.size()) + " behaviour(s)");
}

void ScriptSystem::stop()
{
	if (!m_running) return;

	std::vector<EntityId> entities;
	for (auto& [entity, behaviour] : m_behaviours)
	{
		entities.push_back(entity);
	}
	for (EntityId entity : entities)
	{
		destroyBehaviour(entity);
	}

	m_behaviours.clear();
	m_destroyQueue.clear();
	m_running = false;
	m_world = nullptr;

	ScriptEngine& engine = ScriptEngine::getInstance();
	if (engine.getActiveSystem() == this)
	{
		engine.setActiveSystem(nullptr);
	}

	LOG_INFO("Scripts stopped");
}

EntityId ScriptSystem::spawn(const std::string& primitive, const glm::vec3& position)
{
	if (!m_spawnFunction) return INVALID_ENTITY;
	return m_spawnFunction(primitive, position);
}

void ScriptSystem::requestDestroy(EntityId entity)
{
	if (std::find(m_destroyQueue.begin(), m_destroyQueue.end(), entity) == m_destroyQueue.end())
	{
		m_destroyQueue.push_back(entity);
	}
}

void ScriptSystem::createBehaviour(
	EntityId entity,
	const Script& script)
{
	try
	{
		py::object behaviourClass = ScriptEngine::getInstance().loadBehaviourClass
		(
				script.moduleName,
				script.className
		);

		if (behaviourClass.is_none())
		{
			return;
		}

		py::object behaviour = behaviourClass();

		ScriptBehaviour* nativeBehaviour = behaviour.cast<ScriptBehaviour*>();

		nativeBehaviour->entityId = entity;

		ScriptFields::apply
		(
			behaviour,
			behaviourClass,
			script
		);

		m_behaviours[entity] = std::move(behaviour);
	}
	catch (const py::cast_error&)
	{
		LOG_ERROR
		(
			"Script class " +
			script.moduleName + "." + script.className +
			" must derive from engine.Behaviour"
		);
	}
	catch (const std::exception& e)
	{
		ScriptEngine::logPythonError
		(
			"Failed to create " +
			script.moduleName + "." + script.className,
			e
		);
	}
}

void ScriptSystem::destroyBehaviour(EntityId entity)
{
	auto it = m_behaviours.find(entity);
	if (it == m_behaviours.end()) return;

	py::object behaviour = std::move(it->second);
	m_behaviours.erase(it);

	try
	{
		behaviour.cast<ScriptBehaviour*>()->onDestroy();
	}
	catch (const std::exception& e)
	{
		ScriptEngine::logPythonError("on_destroy of entity " + std::to_string(entity) + " failed", e);
	}
}

void ScriptSystem::flushDestroyQueue()
{
	// on_destroy may request more destroys, so drain until empty
	while (!m_destroyQueue.empty())
	{
		std::vector<EntityId> queue = std::move(m_destroyQueue);
		m_destroyQueue.clear();

		for (EntityId entity : queue)
		{
			if (!m_world->isValidEntity(entity)) continue;

			destroyBehaviour(entity);
			m_world->destroyEntity(entity);
		}
	}
}
