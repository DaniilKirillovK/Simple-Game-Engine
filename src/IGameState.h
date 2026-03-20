#pragma once
#include <memory>
#include <cstdint>

class IRenderAdapter;
class InputHandler;

class IGameState 
{
public:
    virtual ~IGameState() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update(float deltaTime, InputHandler& input) = 0;
    virtual bool isFinished() const { return false; }
    virtual std::unique_ptr<IGameState> getNextState() { return nullptr; }
};