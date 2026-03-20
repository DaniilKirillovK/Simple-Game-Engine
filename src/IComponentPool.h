#pragma once
#include "Entity.h"

class IComponentPool 
{
public:
    virtual ~IComponentPool() = default;
    virtual bool hasComponent(EntityId entity) const = 0;
    virtual void removeComponent(EntityId entity) = 0;
};