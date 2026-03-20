#pragma once

#include "Application.h"
#include "GameplayState.h"
#include "OpenGLRenderAdapter.h"

Application::Application()
{
}

Application::~Application()
{
}

bool Application::initialize(int width, int height, const std::string& title)
{
    LOG_INFO("Initializing application...");

    currentState = std::make_unique<GameplayState>();
    currentState->onEnter();

    renderer = std::make_unique<OpenGLRenderAdapter>();
    if (!renderer->initialize(width, height)) 
    {
        LOG_ERROR("Failed to initialize renderer");
        return false;
    }

    running = true;
    lastTime = std::chrono::high_resolution_clock::now();
    LOG_INFO("Application initialized successfully");
    return true;
}

void Application::run()
{
    LOG_INFO("Entering main loop");
    while (running && !renderer->shouldClose()) 
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        handleEvents();
        update(deltaTime);
        render();
    }
    LOG_INFO("Main loop finished");
}

void Application::shutdown()
{
    LOG_INFO("Shutting down application");
    if (renderer) renderer->shutdown();
}

void Application::handleEvents()
{
}

void Application::update(float deltaTime)
{
}

void Application::render()
{
    renderer->render();
}

void Application::changeState(std::unique_ptr<IGameState> newState)
{
    LOG_INFO("Game state changed");
}
