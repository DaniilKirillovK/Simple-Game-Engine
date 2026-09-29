#pragma once
#include "Entity.h"
#include "ISystem.h"
#include "ComponentPool.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <typeindex>
#include "JobSystem/JobSystem.h"

class World
{
public:
	World() : nextEntityId(1), m_currentDeltaTime(0.0f) {}

	EntityId createEntity();
	void destroyEntity(EntityId entity);
	bool isValidEntity(EntityId entity) const;

	template<typename T>
	T* getComponent(EntityId entity);

	template<typename T>
	ComponentPool<T>& getComponentPool();

	template<typename T>
	T* getSystem();

	float getCurrentDeltaTime() { return m_currentDeltaTime; }

	void addSystem(std::unique_ptr<ISystem> system);
	void update(JobSystem* jobs, float deltaTime);

	void clear();

private:
	float m_currentDeltaTime;
	EntityId nextEntityId;
	std::unordered_set<EntityId> entities;
	std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> componentPools;
	std::vector<std::unique_ptr<ISystem>> systems;
};

inline EntityId World::createEntity()
{
	EntityId id = nextEntityId++;
	entities.insert(id);
	return id;
}

inline void World::destroyEntity(EntityId entity)
{
	for (auto& [type, pool] : componentPools)
	{
		pool->removeComponent(entity);
	}
	entities.erase(entity);
}

inline bool World::isValidEntity(EntityId entity) const
{
	return entities.find(entity) != entities.end();
}

template<typename T>
inline T* World::getComponent(EntityId entity)
{
	ComponentPool<T>& pool = getComponentPool<T>();
	return pool.getComponent(entity);
}

template<typename T>
inline ComponentPool<T>& World::getComponentPool()
{
	std::type_index type = std::type_index(typeid(T));
	auto it = componentPools.find(type);
	if (it == componentPools.end()) 
	{
		auto pool = std::make_unique<ComponentPool<T>>();
		ComponentPool<T>* poolPtr = pool.get();
		componentPools[type] = std::move(pool);
		return *poolPtr;
	}
	return *static_cast<ComponentPool<T>*>(it->second.get());
}

template<typename T>
inline T* World::getSystem()
{
	for (auto& system : systems)
	{
		T* casted = dynamic_cast<T*>(system.get());
		if (casted)
		{
			return casted;
		}
	}
	return nullptr;
}

inline void World::addSystem(std::unique_ptr<ISystem> system)
{
	systems.push_back(std::move(system));
}

inline void World::update(JobSystem* jobs, float deltaTime)
{
	m_currentDeltaTime = deltaTime;
	for (std::unique_ptr<ISystem>& system : systems)
	{
		system->update(*this, jobs, deltaTime);
	}
}

inline void World::clear()
{
	componentPools.clear();
	entities.clear();
	nextEntityId = 1;
	m_currentDeltaTime = 0.0f;
}
