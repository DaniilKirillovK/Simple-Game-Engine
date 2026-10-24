#pragma once

#include "ISystem.h"
#include "Entity.h"

#include <pybind11/pybind11.h>
#include <glm/glm.hpp>
#include "Utils/json.hpp"
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

class Script;

// Runs Python behaviours of entities with a Script component.
// Behaviour instances live only between start() (Play) and stop() (Stop).
class ScriptSystem : public ISystem
{
public:
	using SpawnFunction = std::function<EntityId(const std::string& primitive, const glm::vec3& position)>;
	using PrefabSpawnFunction = std::function<EntityId(const std::string& prefab, const glm::vec3& position,
		const nlohmann::json& fieldOverrides)>;

	ScriptSystem(SpawnFunction spawnFunction, PrefabSpawnFunction prefabSpawnFunction);
	~ScriptSystem();

	virtual void update(World& world, float deltaTime) override;
	virtual void setEnabled(bool isEnabled) override;

	void start(World& world);
	void stop();
	bool isRunning() const { return m_running; }

	// Script -> engine services (called from bindings, only while running)
	World& getWorld() { return *m_world; }
	EntityId spawn(const std::string& primitive, const glm::vec3& position);
	// Spawned behaviours get on_start before their first on_update
	EntityId spawnPrefab(const std::string& prefab, const glm::vec3& position, const nlohmann::json& fieldOverrides);
	void requestDestroy(EntityId entity);
	std::uint64_t getSessionId() const
	{
		return m_sessionId;
	}

	// Live behaviour of an entity or None; lets scripts call each other
	pybind11::object getBehaviour(EntityId entity) const;

	// HUD lines shown over the viewport during Play, ordered by key; cleared on Stop
	void setHudLine(const std::string& key, const std::string& text) { m_hud[key] = text; }
	void clearHud(const std::string& key);
	const std::map<std::string, std::string>& getHud() const { return m_hud; }

private:
	void createBehaviour(EntityId entity, const Script& script);
	void destroyBehaviour(EntityId entity);
	void flushDestroyQueue();
	void activateSpawned(EntityId entity);
	void flushPendingStarts();

	SpawnFunction m_spawnFunction;
	PrefabSpawnFunction m_prefabSpawnFunction;
	World* m_world = nullptr;
	bool m_running = false;
	bool m_isEnabled = true;
	std::uint64_t m_sessionId = 0;

	std::unordered_map<EntityId, pybind11::object> m_behaviours;
	std::vector<EntityId> m_destroyQueue;
	std::vector<EntityId> m_pendingStarts;
	std::map<std::string, std::string> m_hud;
};
