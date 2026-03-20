#pragma once

#include "ISystem.h"
#include "Component.h"
#include <cmath>

class MovementSystem : public ISystem 
{
public:
    void update(World& world, float deltaTime) override;
};