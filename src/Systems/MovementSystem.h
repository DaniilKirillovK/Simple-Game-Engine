#pragma once

#include "ISystem.h"
#include "Component.h"
#include <cmath>

class MovementSystem : public ISystem 
{
public:
    virtual void update(World& world, JobSystem* jobs, float deltaTime) override;
    virtual void setEnabled(bool isEnabled) override;
        
private:
    float m_time = 0.0f;
    bool m_isEnabled = true;
};