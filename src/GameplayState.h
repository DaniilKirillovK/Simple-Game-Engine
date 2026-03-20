#pragma once
#include "IGameState.h"

class GameplayState : public IGameState 
{
public:
    void onEnter() override;
    void update(float deltaTime, InputHandler& input) override;
    void render(IRenderAdapter& renderer) override;
};