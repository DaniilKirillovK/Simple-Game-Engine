#include "ScriptSystem.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptBehaviour.h"
#include "Components/Script.h"
#include "World.h"
#include "Logger.h"
#include "Scripting/ScriptFields.h"
#include "Systems/PhysicsSystem.h"

#include <algorithm>

#include "tracy/Tracy.hpp"

namespace py = pybind11;

constexpr std::uint32_t PYTHON_ZONE_COLOR = 0x3776AB;

ScriptSystem::ScriptSystem(SpawnFunction spawnFunction, PrefabSpawnFunction prefabSpawnFunction)
	: m_spawnFunction(std::move(spawnFunction)),
	m_prefabSpawnFunction(std::move(prefabSpawnFunction))
{
}

ScriptSystem::~ScriptSystem()
{
	if (ScriptEngine::getInstance().isInitialized())
	{
		stop();
		return;
	}

	for (auto& [entity, behaviour] : m_behaviours)
	{
		behaviour.release();
	}
	for (auto& [entity, suspended] : m_suspended)
	{
		suspended.behaviour.release();
	}
}

void ScriptSystem::update(World& world, float deltaTime)
{
	ZoneScopedN("ScriptSystem::update");

	const std::vector<std::string> loadedModules = ScriptEngine::getInstance().pollFileChanges(deltaTime);

	if (!m_running) return;

	m_world = &world;
	applyHotReload(world, loadedModules);

	if (!m_isEnabled) return;

	m_flashRemaining = std::max(0.0f, m_flashRemaining - deltaTime);

	flushPendingStarts();
	dispatchTriggerEvents(world);
	flushPendingStarts();

	std::vector<EntityId> entities;
	entities.reserve(m_behaviours.size());
	for (auto& [entity, behaviour] : m_behaviours)
	{
		entities.push_back(entity);
	}

	ZoneNamedN(updateZone, "ScriptSystem::onUpdate", true);
	ZoneValueV(updateZone, entities.size());

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
		ZoneTransientN(scriptZone, zoneNames(entity).update.c_str(), true);
		ZoneValueV(scriptZone, entity);
		ZoneColorV(scriptZone, PYTHON_ZONE_COLOR);
		try
		{
			behaviour.cast<ScriptBehaviour*>()->onUpdate(deltaTime);
		}
		catch (const std::exception& e)
		{
			ScriptEngine::logPythonError("on_update of entity " + std::to_string(entity)
				+ " failed, behaviour paused until its script is saved again", e);
			suspendBehaviour(entity, true);
		}
	}

	flushPendingStarts();
	flushDestroyQueue();
}

void ScriptSystem::suspendBehaviour(EntityId entity, bool started)
{
	auto it = m_behaviours.find(entity);
	if (it == m_behaviours.end()) return;

	m_suspended[entity] = SuspendedBehaviour{ std::move(it->second), started };
	m_behaviours.erase(it);
}

void ScriptSystem::applyHotReload(World& world, const std::vector<std::string>& modules)
{
	if (modules.empty()) return;

	ZoneScopedN("ScriptSystem::applyHotReload");

	m_unavailableScripts.clear();

	const std::unordered_set<std::string> changedModules(modules.begin(), modules.end());

	std::vector<std::pair<EntityId, Script>> scripts;
	auto& scriptPool = world.getComponentPool<Script>();
	for (size_t i = 0; i < scriptPool.size(); ++i)
	{
		if (changedModules.count(scriptPool.components()[i].moduleName))
		{
			scripts.emplace_back(scriptPool.entities()[i], scriptPool.components()[i]);
		}
	}

	std::map<std::string, py::object> classes;
	std::vector<EntityId> swapped;

	for (const auto& [entity, script] : scripts)
	{
		const std::string scriptName = script.moduleName + "." + script.className;

		auto classIt = classes.find(scriptName);
		if (classIt == classes.end())
		{
			py::object behaviourClass = ScriptEngine::getInstance().loadBehaviourClass
			(
				script.moduleName,
				script.className,
				true
			);
			classIt = classes.emplace(scriptName, std::move(behaviourClass)).first;
		}

		if (classIt->second.is_none())
		{
			continue;
		}

		auto liveIt = m_behaviours.find(entity);
		auto suspendedIt = m_suspended.find(entity);
		const bool suspended = suspendedIt != m_suspended.end();

		if (liveIt == m_behaviours.end() && !suspended)
		{
			activateSpawned(entity);
			continue;
		}

		py::object behaviour = suspended ? suspendedIt->second.behaviour : liveIt->second;
		bool started = suspended ? suspendedIt->second.started : true;

		try
		{
			ScriptFields::swapClass(behaviour, classIt->second, script);
		}
		catch (const std::exception& e)
		{
			ScriptEngine::logPythonError("Hot reload of " + scriptName + " on entity "
				+ std::to_string(entity) + " failed, the old version keeps running", e);
			continue;
		}

		if (suspended)
		{
			m_suspended.erase(suspendedIt);
			m_behaviours[entity] = behaviour;
			if (!started)
			{
				m_pendingStarts.push_back(entity);
			}
		}
		else
		{
			started = std::find(m_pendingStarts.begin(), m_pendingStarts.end(), entity) == m_pendingStarts.end();
		}

		if (started)
		{
			swapped.push_back(entity);
		}
	}

	for (EntityId entity : swapped)
	{
		notifyReloaded(entity);
	}

	if (!swapped.empty())
	{
		LOG_INFO("Hot reload applied to " + std::to_string(swapped.size()) + " running object(s)");
	}
}

void ScriptSystem::notifyReloaded(EntityId entity)
{
	auto it = m_behaviours.find(entity);
	if (it == m_behaviours.end()) return;

	py::object behaviour = it->second;
	if (!py::hasattr(behaviour, "on_reload")) return;

	ZoneTransientN(scriptZone, zoneNames(entity).reload.c_str(), true);
	ZoneValueV(scriptZone, entity);
	ZoneColorV(scriptZone, PYTHON_ZONE_COLOR);

	try
	{
		behaviour.attr("on_reload")();
	}
	catch (const std::exception& e)
	{
		ScriptEngine::logPythonError("on_reload of entity " + std::to_string(entity)
			+ " failed, behaviour paused until its script is saved again", e);
		suspendBehaviour(entity, true);
	}
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
	m_unavailableScripts.clear();
	m_world = &world;
	m_running = true;

	engine.setActiveSystem(this);

	std::vector<std::pair<EntityId, Script>> scripts;
	auto& scriptPool = world.getComponentPool<Script>();
	for (size_t i = 0; i < scriptPool.size(); ++i)
	{
		scripts.emplace_back(scriptPool.entities()[i], scriptPool.components()[i]);
	}

	for (const auto& [entity, script] : scripts)
	{
		createBehaviour(entity, script, false);
	}

	for (const auto& [entity, script] : scripts)
	{
		auto it = m_behaviours.find(entity);
		if (it == m_behaviours.end()) continue;

		py::object behaviour = it->second;
		ZoneTransientN(scriptZone, zoneNames(entity).start.c_str(), true);
		ZoneValueV(scriptZone, entity);
		ZoneColorV(scriptZone, PYTHON_ZONE_COLOR);
		try
		{
			behaviour.cast<ScriptBehaviour*>()->onStart();
		}
		catch (const std::exception& e)
		{
			ScriptEngine::logPythonError("on_start of entity " + std::to_string(entity)
				+ " failed, behaviour paused until its script is saved again", e);
			suspendBehaviour(entity, false);
		}
	}

	flushPendingStarts();
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
	m_suspended.clear();
	m_zoneNames.clear();
	m_destroyQueue.clear();
	m_pendingStarts.clear();
	m_hud.clear();
	m_hudCenter.clear();
	m_flashAlpha = 0.0f;
	m_flashDuration = 0.0f;
	m_flashRemaining = 0.0f;
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

EntityId ScriptSystem::spawnPrefab(const std::string& prefab, const glm::vec3& position,
	const nlohmann::json& fieldOverrides)
{
	if (!m_prefabSpawnFunction) return INVALID_ENTITY;

	EntityId entity = m_prefabSpawnFunction(prefab, position, fieldOverrides);
	if (entity != INVALID_ENTITY && m_running)
	{
		activateSpawned(entity);
	}
	return entity;
}

void ScriptSystem::activateSpawned(EntityId entity)
{
	Script* script = m_world->getComponent<Script>(entity);
	if (!script) return;

	const Script scriptCopy = *script;
	createBehaviour(entity, scriptCopy, true);

	if (m_behaviours.count(entity))
	{
		m_pendingStarts.push_back(entity);
	}
}

void ScriptSystem::flushPendingStarts()
{
	if (m_pendingStarts.empty()) return;

	ZoneScopedN("ScriptSystem::flushPendingStarts");

	constexpr int MAX_START_ROUNDS = 16;
	for (int round = 0; round < MAX_START_ROUNDS && !m_pendingStarts.empty(); ++round)
	{
		std::vector<EntityId> pending = std::move(m_pendingStarts);
		m_pendingStarts.clear();

		for (EntityId entity : pending)
		{
			auto it = m_behaviours.find(entity);
			if (it == m_behaviours.end()) continue;

			py::object behaviour = it->second;
			ZoneTransientN(scriptZone, zoneNames(entity).start.c_str(), true);
			ZoneValueV(scriptZone, entity);
			ZoneColorV(scriptZone, PYTHON_ZONE_COLOR);
			try
			{
				behaviour.cast<ScriptBehaviour*>()->onStart();
			}
			catch (const std::exception& e)
			{
				ScriptEngine::logPythonError("on_start of spawned entity " + std::to_string(entity)
					+ " failed, behaviour paused until its script is saved again", e);
				suspendBehaviour(entity, false);
			}
		}
	}
}

const ScriptSystem::ZoneNames& ScriptSystem::zoneNames(EntityId entity) const
{
	static const ZoneNames unknown{ "Script.on_start", "Script.on_update", "Script.on_destroy",
		"Script.on_trigger_enter", "Script.on_trigger_exit", "Script.on_reload", "Script.on_field_changed" };

	auto it = m_zoneNames.find(entity);
	return it != m_zoneNames.end() ? it->second : unknown;
}

void ScriptSystem::dispatchTriggerEvents(World& world)
{
	PhysicsSystem* physics = world.getSystem<PhysicsSystem>();
	if (!physics || physics->getTriggerEvents().empty()) return;

	ZoneScopedN("ScriptSystem::triggerEvents");

	const std::vector<TriggerEvent> events = physics->getTriggerEvents();

	py::object resolve;
	try
	{
		resolve = py::module_::import("engine").attr("resolve_entity");
	}
	catch (const std::exception& e)
	{
		ScriptEngine::logPythonError("Trigger events are unavailable", e);
		return;
	}

	for (const TriggerEvent& event : events)
	{
		deliverTriggerEvent(event.trigger, event.other, event.entered, resolve);
		deliverTriggerEvent(event.other, event.trigger, event.entered, resolve);
	}
}

void ScriptSystem::deliverTriggerEvent(EntityId target, EntityId other, bool entered, const py::object& resolve)
{
	auto it = m_behaviours.find(target);
	if (it == m_behaviours.end()) return;

	const char* method = entered ? "on_trigger_enter" : "on_trigger_exit";
	py::object behaviour = it->second;
	if (!py::hasattr(behaviour, method)) return;

	const ZoneNames& names = zoneNames(target);
	ZoneTransientN(scriptZone, (entered ? names.triggerEnter : names.triggerExit).c_str(), true);
	ZoneValueV(scriptZone, target);
	ZoneColorV(scriptZone, PYTHON_ZONE_COLOR);

	try
	{
		py::object handle = resolve(other, m_sessionId);
		if (handle.is_none()) return;
		behaviour.attr(method)(handle);
	}
	catch (const std::exception& e)
	{
		ScriptEngine::logPythonError(std::string(method) + " of entity " + std::to_string(target)
			+ " failed, behaviour paused until its script is saved again", e);
		suspendBehaviour(target, true);
	}
}

py::object ScriptSystem::getBehaviour(EntityId entity) const
{
	auto it = m_behaviours.find(entity);
	return it != m_behaviours.end() ? it->second : py::none();
}

void ScriptSystem::applyFieldEdits(EntityId entity, const Script& script, const nlohmann::json& previousFields)
{
	auto liveIt = m_behaviours.find(entity);
	auto suspendedIt = m_suspended.find(entity);
	const bool live = liveIt != m_behaviours.end();

	if (!live && suspendedIt == m_suspended.end()) return;

	py::object behaviour = live ? liveIt->second : suspendedIt->second.behaviour;

	std::vector<std::string> edited;
	try
	{
		edited = ScriptFields::applyEdits(behaviour, previousFields, script);
	}
	catch (const std::exception& e)
	{
		ScriptEngine::logPythonError("Failed to apply fields to entity " + std::to_string(entity), e);
		return;
	}

	if (!live || edited.empty() || !py::hasattr(behaviour, "on_field_changed")) return;
	if (std::find(m_pendingStarts.begin(), m_pendingStarts.end(), entity) != m_pendingStarts.end()) return;

	ZoneTransientN(scriptZone, zoneNames(entity).fieldChanged.c_str(), true);
	ZoneValueV(scriptZone, entity);
	ZoneColorV(scriptZone, PYTHON_ZONE_COLOR);

	try
	{
		for (const std::string& name : edited)
		{
			behaviour.attr("on_field_changed")(name);
		}
	}
	catch (const std::exception& e)
	{
		ScriptEngine::logPythonError("on_field_changed of entity " + std::to_string(entity)
			+ " failed, behaviour paused until its script is saved again", e);
		suspendBehaviour(entity, true);
	}
}

void ScriptSystem::clearHud(const std::string& key)
{
	if (key.empty())
	{
		m_hud.clear();
	}
	else
	{
		m_hud.erase(key);
	}
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
	const Script& script,
	bool allowLastGood)
{
	m_suspended.erase(entity);

	const std::string createZoneName = script.className + ".create";
	ZoneTransientN(createZone, createZoneName.c_str(), true);
	ZoneValueV(createZone, entity);
	ZoneColorV(createZone, PYTHON_ZONE_COLOR);

	try
	{
		py::object behaviourClass = ScriptEngine::getInstance().loadBehaviourClass
		(
				script.moduleName,
				script.className,
				allowLastGood
		);

		if (behaviourClass.is_none())
		{
			const std::string scriptName = script.moduleName + "." + script.className;
			if (m_unavailableScripts.insert(scriptName).second)
			{
				LOG_ERROR("Script '" + scriptName + "' is not available (see the error above): "
					"objects using it run without behaviour until it is fixed");
			}
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
		m_zoneNames[entity] = ZoneNames{
			script.className + ".on_start",
			script.className + ".on_update",
			script.className + ".on_destroy",
			script.className + ".on_trigger_enter",
			script.className + ".on_trigger_exit",
			script.className + ".on_reload",
			script.className + ".on_field_changed"
		};
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
	m_suspended.erase(entity);

	auto it = m_behaviours.find(entity);
	if (it == m_behaviours.end()) return;

	py::object behaviour = std::move(it->second);
	m_behaviours.erase(it);

	const std::string destroyZoneName = zoneNames(entity).destroy;
	m_zoneNames.erase(entity);

	ZoneTransientN(scriptZone, destroyZoneName.c_str(), true);
	ZoneValueV(scriptZone, entity);
	ZoneColorV(scriptZone, PYTHON_ZONE_COLOR);

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
	if (m_destroyQueue.empty()) return;

	ZoneScopedN("ScriptSystem::flushDestroyQueue");

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
