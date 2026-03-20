#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include "Logger.h"
#include "IGameState.h"
#include "InputHandler.h"
#include "IRenderAdapter.h"

class Application
{
public:
    Application();
    ~Application();
    bool initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();

private:
    void handleEvents();
    void update(float deltaTime);
    void render();
    void changeState(std::unique_ptr<IGameState> newState);

    std::unique_ptr<IRenderAdapter> renderer;
    std::unique_ptr<IGameState> currentState;
    InputHandler input;
    bool running = false;
    std::chrono::high_resolution_clock::time_point lastTime;
};