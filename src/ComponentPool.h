#pragma once
#include "IComponentPool.h"
#include <unordered_map>

template<typename T>
class ComponentPool : public IComponentPool
{
public:
	static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

	void addComponent(EntityId entity, const T& component);
	T* getComponent(EntityId entity);
	bool hasComponent(EntityId entity) const override;
	void removeComponent(EntityId entity) override;

	size_t size() const { return m_components.size(); }

	std::vector<T>& components() { return m_components; }
	const std::vector<T>& components() const { return m_components; }

	std::vector<EntityId>& entities() { return m_componentsEntities; }
	const std::vector<EntityId>& entities() const { return m_componentsEntities; }

private:
	std::vector<T> m_components; // components
	std::vector<EntityId> m_componentsEntities; // EntityId[i] for m_components[i]
	std::vector<uint32_t> m_componentsIdx; // entity → idx in m_components (or INVALID)
};

template<typename T>
inline void ComponentPool<T>::addComponent(EntityId entity, const T& component)
{
	if (entity >= m_componentsIdx.size())
	{
		m_componentsIdx.resize(entity + 1, INVALID_INDEX);
	}

	uint32_t idx = m_componentsIdx[entity];
	if (idx != INVALID_INDEX)
	{
		m_components[idx] = component;
		return;
	}

	m_componentsIdx[entity] = static_cast<uint32_t>(m_components.size());
	m_components.push_back(component);
	m_componentsEntities.push_back(entity);
}

template<typename T>
inline T* ComponentPool<T>::getComponent(EntityId entity)
{
	if (entity >= m_componentsIdx.size()) return nullptr;
	uint32_t idx = m_componentsIdx[entity];
	return idx != INVALID_INDEX ? &m_components[idx] : nullptr;
}

template<typename T>
inline bool ComponentPool<T>::hasComponent(EntityId entity) const
{
	return entity < m_componentsIdx.size() && m_componentsIdx[entity] != INVALID_INDEX;
}

template<typename T>
inline void ComponentPool<T>::removeComponent(EntityId entity)
{
	if (entity >= m_componentsIdx.size()) return;
	uint32_t idx = m_componentsIdx[entity];
	if (idx == INVALID_INDEX) return;

	uint32_t last = static_cast<uint32_t>(m_components.size() - 1);
	if (idx != last)
	{
		m_components[idx] = std::move(m_components[last]);
		m_componentsEntities[idx] = m_componentsEntities[last];
		m_componentsIdx[m_componentsEntities[idx]] = idx;
	}
	m_components.pop_back();
	m_componentsEntities.pop_back();
	m_componentsIdx[entity] = INVALID_INDEX;
}