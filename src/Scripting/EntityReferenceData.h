#pragma once

#include "Entity.h"
#include "Utils/json.hpp"

#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace EntityReferenceData
{
    using Json = nlohmann::json;

    inline bool isReference(const Json& value)
    {
        if (!value.is_object() ||
            value.size() != 1 ||
            !value.contains("$entity"))
        {
            return false;
        }

        const Json& id = value.at("$entity");

        if (!id.is_number_integer())
        {
            return false;
        }

        const double number = id.get<double>();

        return number > 0.0 && number <= static_cast<double>(std::numeric_limits<EntityId>::max());
    }

    inline EntityId read(const Json& value)
    {
        if (value.is_null())
        {
            return INVALID_ENTITY;
        }

        if (!isReference(value))
        {
            throw std::runtime_error("Expected null or an entity reference");
        }

        return value.at("$entity").get<EntityId>();
    }

    inline Json make(EntityId id)
    {
        if (id == INVALID_ENTITY)
        {
            return nullptr;
        }

        return Json
        {
            { "$entity", id }
        };
    }

    inline void remapFields(
        Json& fields,
        const std::unordered_map<EntityId, EntityId>& idMap)
    {
        if (!fields.is_object())
        {
            return;
        }

        for (auto& item : fields.items())
        {
            Json& value = item.value();

            if (!value.is_object() ||
                !value.contains("$entity"))
            {
                continue;
            }

            const EntityId oldId = read(value);
            const auto it = idMap.find(oldId);

            if (it == idMap.end())
            {
                value = nullptr;
            }
            else
            {
                value = make(it->second);
            }
        }
    }
}