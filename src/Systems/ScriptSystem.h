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
#include <unordered_set>
#include <vector>
#include <cstdint>

class Script;

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

	void requestRestart() { m_restartRequested = true; }
	bool consumeRestartRequest()
	{
		const bool requested = m_restartRequested;
		m_restartRequested = false;
		return requested;
	}

	World& getWorld() { return *m_world; }
	EntityId spawn(const std::string& primitive, const glm::vec3& position);
	EntityId spawnPrefab(const std::string& prefab, const glm::vec3& position, const nlohmann::json& fieldOverrides);
	void requestDestroy(EntityId entity);
	std::uint64_t getSessionId() const
	{
		return m_sessionId;
	}

	pybind11::object getBehaviour(EntityId entity) const;
	void applyFieldEdits(EntityId entity, const Script& script, const nlohmann::json& previousFields);

	void setHudLine(const std::string& key, const std::string& text) { m_hud[key] = text; }
	void clearHud(const std::string& key);
	const std::map<std::string, std::string>& getHud() const { return m_hud; }
	void setHudCenter(const std::string& text) { m_hudCenter = text; }
	const std::string& getHudCenter() const { return m_hudCenter; }

	void setScreenFlash(const glm::vec3& color, float alpha, float duration)
	{
		m_flashColor = color;
		m_flashAlpha = alpha;
		m_flashDuration = duration;
		m_flashRemaining = duration;
	}
	const glm::vec3& getScreenFlashColor() const { return m_flashColor; }
	float getScreenFlashAlpha() const
	{
		if (m_flashDuration <= 0.0f) return m_flashAlpha;
		return m_flashAlpha * (m_flashRemaining / m_flashDuration);
	}

private:
	void createBehaviour(EntityId entity, const Script& script, bool allowLastGood);
	void destroyBehaviour(EntityId entity);
	void suspendBehaviour(EntityId entity, bool started);
	void applyHotReload(World& world, const std::vector<std::string>& modules);
	void notifyReloaded(EntityId entity);
	void flushDestroyQueue();
	void activateSpawned(EntityId entity);
	void flushPendingStarts();
	void dispatchTriggerEvents(World& world);
	void deliverTriggerEvent(EntityId target, EntityId other, bool entered, const pybind11::object& resolve);

	struct ZoneNames
	{
		std::string start;
		std::string update;
		std::string destroy;
		std::string triggerEnter;
		std::string triggerExit;
		std::string reload;
		std::string fieldChanged;
	};
	const ZoneNames& zoneNames(EntityId entity) const;

	struct SuspendedBehaviour
	{
		pybind11::object behaviour;
		bool started = false;
	};

	SpawnFunction m_spawnFunction;
	PrefabSpawnFunction m_prefabSpawnFunction;
	World* m_world = nullptr;
	bool m_running = false;
	bool m_isEnabled = true;
	std::uint64_t m_sessionId = 0;

	std::unordered_map<EntityId, pybind11::object> m_behaviours;
	std::unordered_map<EntityId, SuspendedBehaviour> m_suspended;
	std::unordered_map<EntityId, ZoneNames> m_zoneNames;
	std::unordered_set<std::string> m_unavailableScripts;
	std::vector<EntityId> m_destroyQueue;
	std::vector<EntityId> m_pendingStarts;
	std::map<std::string, std::string> m_hud;
	std::string m_hudCenter;
	glm::vec3 m_flashColor = glm::vec3(1.0f, 0.0f, 0.0f);
	float m_flashAlpha = 0.0f;
	float m_flashDuration = 0.0f;
	float m_flashRemaining = 0.0f;
	bool m_restartRequested = false;
};
