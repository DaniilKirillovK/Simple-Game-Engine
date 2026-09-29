#pragma once
#include <memory>
#include <cstdint>

class JobSystem;
class IRenderAdapter;
class InputHandler;

class IGameState 
{
public:
    virtual ~IGameState() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update(JobSystem* jobs, float deltaTime) = 0;
    virtual void render() = 0;
    virtual bool isFinished() const { return false; }
    virtual std::unique_ptr<IGameState> getNextState() { return nullptr; }
};