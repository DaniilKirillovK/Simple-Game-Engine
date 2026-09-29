#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include "Logger.h"
#include "IGameState.h"
#include "InputHandler.h"
#include "IRenderAdapter.h"
#include "JobSystem/JobSystem.h"
#include "World.h"

class Application
{
public:
    Application() : m_jobs(0) {}
    ~Application();
    bool initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();

    JobSystem* jobs() { return m_jobs; }

private:
    void bindActions();
    void handleEvents();
    void update(float deltaTime);
    void render();
    void changeState(std::unique_ptr<IGameState> newState);

    JobSystem* m_jobs;
    std::unique_ptr<IRenderAdapter> renderer;
    std::unique_ptr<IGameState> currentState;
    bool running = false;
    std::chrono::high_resolution_clock::time_point lastTime;
};