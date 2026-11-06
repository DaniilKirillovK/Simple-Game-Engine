#include "Systems/TriggerTracker.h"
#include "World.h"
#include "Components/Collider.h"
#include "Utils/EntityBounds.h"

#include "tracy/Tracy.hpp"

void TriggerTracker::update(World& world)
{
    ZoneScopedN("Physics::Triggers");

    m_events.clear();

    struct Item
    {
        EntityId entity;
        Bounds bounds;
        bool trigger;
    };

    std::vector<Item> items;
    auto& colliders = world.getComponentPool<Collider>();
    items.reserve(colliders.size());
    for (size_t i = 0; i < colliders.size(); ++i)
    {
        const EntityId entity = colliders.entities()[i];
        Item item{ entity, {}, colliders.components()[i].isTrigger };
        if (EntityBounds::compute(world, entity, item.bounds))
        {
            items.push_back(item);
        }
    }

    std::set<std::pair<EntityId, EntityId>> current;
    for (const Item& trigger : items)
    {
        if (!trigger.trigger) continue;

        for (const Item& other : items)
        {
            if (other.trigger || other.entity == trigger.entity) continue;

            if (EntityBounds::overlap(trigger.bounds, other.bounds))
            {
                current.insert({ trigger.entity, other.entity });
            }
        }
    }

    for (const auto& pair : m_pairs)
    {
        if (current.count(pair)) continue;
        if (!world.isValidEntity(pair.first) || !world.isValidEntity(pair.second)) continue;
        m_events.push_back({ pair.first, pair.second, false });
    }

    for (const auto& pair : current)
    {
        if (!m_pairs.count(pair))
        {
            m_events.push_back({ pair.first, pair.second, true });
        }
    }

    m_pairs = std::move(current);

    m_touching.clear();
    for (const auto& pair : m_pairs)
    {
        m_touching.insert(pair.first);
        m_touching.insert(pair.second);
    }
}

void TriggerTracker::clear()
{
    m_pairs.clear();
    m_events.clear();
    m_touching.clear();
}
