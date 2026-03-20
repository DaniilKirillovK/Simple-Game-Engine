#pragma once
#include "IComponentPool.h"
#include <unordered_map>

template<typename T>
class ComponentPool : public IComponentPool
{
public:
	void addComponent(EntityId entity, const T& component);
	T* getComponent(EntityId entity);
	bool hasComponent(EntityId entity) const override;
	void removeComponent(EntityId entity) override;
	std::unordered_map<EntityId, T>& getAll();

private:
	std::unordered_map<EntityId, T> components;
};

template<typename T>
inline void ComponentPool<T>::addComponent(EntityId entity, const T& component)
{
	components[entity] = component;
}

template<typename T>
inline T* ComponentPool<T>::getComponent(EntityId entity)
{
	auto it = components.find(entity);
	return it != components.end() ? &it->second : nullptr;
}

template<typename T>
inline bool ComponentPool<T>::hasComponent(EntityId entity) const
{
	return components.find(entity) != components.end();
}

template<typename T>
inline void ComponentPool<T>::removeComponent(EntityId entity)
{
	components.erase(entity);
}

template<typename T>
inline std::unordered_map<EntityId, T>& ComponentPool<T>::getAll()
{
	return components;
}
