#pragma once

#include "ISystem.h"
#include "Entity.h"

#include <pybind11/pybind11.h>
#include <glm/glm.hpp>
#include <functional>
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

	explicit ScriptSystem(SpawnFunction spawnFunction);
	~ScriptSystem();

	virtual void update(World& world, float deltaTime) override;
	virtual void setEnabled(bool isEnabled) override;

	void start(World& world);
	void stop();
	bool isRunning() const { return m_running; }

	// Script -> engine services (called from bindings, only while running)
	World& getWorld() { return *m_world; }
	EntityId spawn(const std::string& primitive, const glm::vec3& position);
	void requestDestroy(EntityId entity);
	std::uint64_t getSessionId() const
	{
		return m_sessionId;
	}

private:
	void createBehaviour(EntityId entity, const Script& script);
	void destroyBehaviour(EntityId entity);
	void flushDestroyQueue();

	SpawnFunction m_spawnFunction;
	World* m_world = nullptr;
	bool m_running = false;
	bool m_isEnabled = true;
	std::uint64_t m_sessionId = 0;

	std::unordered_map<EntityId, pybind11::object> m_behaviours;
	std::vector<EntityId> m_destroyQueue;
};
