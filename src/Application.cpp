#pragma once

#include "Application.h"
#include "GameplayState.h"
#include "OpenGLRenderAdapter.h"
#include "Resources/ResourceManager.h"
#include "Resources/MeshLoader/MeshLoader.h"
#include "Resources/TextureLoader/TextureLoader.h"
#include "Resources/ShaderLoader/ShaderLoader.h"

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

    MeshLoader::getInstance().registerLoader();
	TextureLoader::getInstance().registerLoader(renderer.get());
    ShaderLoader::getInstance().registerLoader(renderer.get());

    currentState = std::make_unique<GameplayState>(*renderer);
    currentState->onEnter();

    bindActions();

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

void Application::bindActions()
{
    InputHandler::getInstance().bindAction("MoveForward", KeyCode::KEY_W);
    InputHandler::getInstance().bindAction("MoveBack", KeyCode::KEY_S);
    InputHandler::getInstance().bindAction("MoveLeft", KeyCode::KEY_A);
    InputHandler::getInstance().bindAction("MoveRight", KeyCode::KEY_D);
    InputHandler::getInstance().bindAction("MoveUp", KeyCode::KEY_SPACE);
    InputHandler::getInstance().bindAction("MoveDown", KeyCode::KEY_LEFT_CONTROL);
    InputHandler::getInstance().bindAction("ToggleDebug", KeyCode::KEY_F3);
}

void Application::handleEvents()
{
    InputHandler::getInstance().update();
    renderer->pollEvents();

    auto keyEvents = renderer->getKeyEvents();
    auto mouseButtonEvents = renderer->getMouseButtonEvents();
    auto mouseMoveEvents = renderer->getMouseMoveEvents();
    auto mouseScrollEvents = renderer->getMouseScrollEvents();

    InputHandler::getInstance().processKeyEvents(keyEvents);
    InputHandler::getInstance().processMouseButtonEvents(mouseButtonEvents);
    InputHandler::getInstance().processMouseMoveEvents(mouseMoveEvents);
    InputHandler::getInstance().processMouseScrollEvents(mouseScrollEvents);
}

void Application::update(float deltaTime)
{
    if (currentState) 
    {
        currentState->update(deltaTime);
        if (currentState->isFinished()) 
        {
            auto next = currentState->getNextState();
            if (next) changeState(std::move(next));
        }
    }

    InputHandler::getInstance().clearFrameState();
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
