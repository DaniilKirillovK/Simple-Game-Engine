#pragma once

#include "Application.h"
#include "GameplayState.h"
#include "OpenGLRenderAdapter.h"
#include "Resources/ResourceManager.h"

Application::Application()
{
}

Application::~Application()
{
}

bool Application::initialize(int width, int height, const std::string& title)
{
    LOG_INFO("Initializing application...");

    renderer = std::make_unique<OpenGLRenderAdapter>();
    if (!renderer->initialize(width, height)) 
    {
        LOG_ERROR("Failed to initialize renderer");
        return false;
    }

    currentState = std::make_unique<GameplayState>(*renderer, input);
    currentState->onEnter();

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
    renderer->pollEvents();

    auto keyEvents = renderer->getKeyEvents();
    auto mouseButtonEvents = renderer->getMouseButtonEvents();
    auto mouseMoveEvents = renderer->getMouseMoveEvents();
    auto mouseScrollEvents = renderer->getMouseScrollEvents();

    input.processKeyEvents(keyEvents);
    input.processMouseButtonEvents(mouseButtonEvents);
    input.processMouseMoveEvents(mouseMoveEvents);
    input.processMouseScrollEvents(mouseScrollEvents);
}

void Application::update(float deltaTime)
{
    input.update();

    if (currentState) 
    {
        currentState->update(deltaTime);
        if (currentState->isFinished()) 
        {
            auto next = currentState->getNextState();
            if (next) changeState(std::move(next));
        }
    }

    input.clearFrameState();
}

void Application::render()
{
    currentState->render();
}

void Application::changeState(std::unique_ptr<IGameState> newState)
{
    if (currentState) currentState->onExit();
    currentState = std::move(newState);
    if (currentState) currentState->onEnter();

    LOG_INFO("Game state changed");
}
