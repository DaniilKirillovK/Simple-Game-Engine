#pragma once
#include "ISystem.h"
#include "World.h"

class TransformSystem : public ISystem
{
public:
    void update(World& world, float deltaTime) override;

private:
    void updateWorldMatrices(World& world);
};