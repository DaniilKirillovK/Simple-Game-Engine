#pragma once
#include "Entity.h"
#include "ISystem.h"
#include "ComponentPool.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <typeindex>

class World
{
public:
	World() : nextEntityId(1) {}

	EntityId createEntity();
	void destroyEntity(EntityId entity);
	bool isValidEntity(EntityId entity) const;

	template<typename T>
	void addComponent(EntityId entity, const T& component);
	template<typename T>
	void removeComponent(EntityId entity);
	template<typename T>
	T* getComponent(EntityId entity);
	template<typename T>
	bool hasComponent(EntityId entity) const;

	template<typename T>
	std::vector<std::pair<EntityId, T*>> getEntitiesWithComponent();

	void addSystem(std::unique_ptr<ISystem> system);
	void update(float deltaTime);

private:
	template<typename T>
	ComponentPool<T>& getComponentPool();

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
inline void World::addComponent(EntityId entity, const T& component)
{
	static_assert(std::is_base_of_v<Component, T> || true, "T must be a Component");

	ComponentPool<T>& pool = getComponentPool<T>();
	pool.addComponent(entity, component);
}

template<typename T>
inline void World::removeComponent(EntityId entity)
{
	auto it = componentPools.find(std::type_index(typeid(T)));
	if (it != componentPools.end())
	{
		it->second->removeComponent(entity);
	}
}

template<typename T>
inline T* World::getComponent(EntityId entity)
{
	ComponentPool<T>& pool = getComponentPool<T>();
	return pool.getComponent(entity);
}

template<typename T>
inline bool World::hasComponent(EntityId entity) const
{
	auto it = componentPools.find(std::type_index(typeid(T)));
	if (it == componentPools.end()) return false;
	return it->second->hasComponent(entity);
}

template<typename T>
inline std::vector<std::pair<EntityId, T*>> World::getEntitiesWithComponent()
{
	std::vector<std::pair<EntityId, T*>> result;
	auto& pool = getComponentPool<T>();

	for (auto& [entity, component] : pool.getAll()) 
	{
		result.emplace_back(entity, &component);
	}

	return result;
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

inline void World::addSystem(std::unique_ptr<ISystem> system)
{
	systems.push_back(std::move(system));
}

inline void World::update(float deltaTime)
{
	for (std::unique_ptr<ISystem>& system : systems)
	{
		system->update(*this, deltaTime);
	}
}