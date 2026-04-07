#pragma once
#include "IGameState.h"
#include "World.h"

class GameplayState : public IGameState 
{
public:
    GameplayState(IRenderAdapter& renderer, InputHandler& input) 
    : renderer(renderer), input(input) {};

    virtual void onEnter() override;
    virtual void update(float deltaTime) override;
    virtual void render() override;

private:
    void setupTestScene();

    IRenderAdapter& renderer;
    InputHandler& input;

    World world;
};