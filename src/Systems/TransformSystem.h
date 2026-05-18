#pragma once
#include "ISystem.h"
#include "World.h"

class TransformSystem : public ISystem
{
public:
    virtual void update(World& world, float deltaTime) override;
    virtual void setEnabled(bool isEnabled) override;

private:
    bool m_isEnabled = true;
    void updateWorldMatrices(World& world);
};