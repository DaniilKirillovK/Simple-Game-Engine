#pragma once

#include "ISystem.h"
#include "Component.h"
#include <cmath>

class MovementSystem : public ISystem 
{
public:
    virtual void update(World& world, float deltaTime) override;
    virtual void setEnabled(bool isEnabled) override;
        
private:
    bool m_isEnabled = true;
};