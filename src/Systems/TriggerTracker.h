#pragma once
#include "Entity.h"

#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

class World;

struct TriggerEvent
{
    EntityId trigger = INVALID_ENTITY;
    EntityId other = INVALID_ENTITY;
    bool entered = false;
};

class TriggerTracker
{
public:
    void update(World& world);
    void clear();

    const std::vector<TriggerEvent>& events() const { return m_events; }
    const std::set<std::pair<EntityId, EntityId>>& pairs() const { return m_pairs; }
    bool isTouching(EntityId entity) const { return m_touching.count(entity) > 0; }

private:
    std::set<std::pair<EntityId, EntityId>> m_pairs;
    std::vector<TriggerEvent> m_events;
    std::unordered_set<EntityId> m_touching;
};
