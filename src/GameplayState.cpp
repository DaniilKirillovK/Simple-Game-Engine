#include "GameplayState.h"
#include "Logger.h"
#include "IRenderAdapter.h"

#include "Systems/RenderSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/TransformSystem.h"

#include <filesystem>
#include "Utils/Serialization/SceneSerializer.h"

GameplayState::GameplayState(IRenderAdapter& renderer)
: m_renderer(renderer)
{
    m_renderer.setOnPlayCallback([this]() {
        enterPlayMode();
        });

    m_renderer.setOnStopCallback([this]() {
        exitPlayMode();
        });
}

void GameplayState::onEnter()
{
    m_world = new World();

    m_world->addSystem(std::make_unique<TransformSystem>());
    m_world->addSystem(std::make_unique<PhysicsSystem>(&m_renderer));
    m_world->addSystem(std::make_unique<RenderSystem>(&m_renderer));
    m_world->addSystem(std::make_unique<CameraSystem>());
    m_world->addSystem(std::make_unique<MovementSystem>());

	setupTestScene();

	LOG_INFO("Enter Gameplay State");
}

void GameplayState::update(float deltaTime)
{
    m_world->update(deltaTime);
}

void GameplayState::render()
{
    m_renderer.render(m_world);
}

void GameplayState::enterPlayMode()
{
    LOG_INFO("Entering PLAY mode");

    saveSceneState();

    if (PhysicsSystem* physicsSystem = m_world->getSystem<PhysicsSystem>())
    {
        physicsSystem->setEnabled(true);
    }

    m_playModeActive = true;
}

void GameplayState::exitPlayMode()
{
    LOG_INFO("Exiting PLAY mode, restoring scene state");

    if (PhysicsSystem* physicsSystem = m_world->getSystem<PhysicsSystem>())
    {
        physicsSystem->setEnabled(false);
    }

    m_playModeActive = false;
    restoreSceneState();
}

void GameplayState::setupTestScene()
{
    std::string scenePath = "assets/scenes/test_scene.json";

    if (std::filesystem::exists(scenePath)) 
    {
        if (SceneSerializer::loadScene(m_renderer, *m_world, scenePath))
        {
            LOG_INFO("Scene loaded successfully from: " + scenePath);
        }
    }

    LOG_INFO("3D scene setup complete");
}

void GameplayState::saveSceneState()
{
    m_savedScenePath = "assets/scenes/temp_save.json";
    SceneSerializer::saveScene(*m_world, m_savedScenePath);
    LOG_INFO("Scene state saved");
}

void GameplayState::restoreSceneState()
{
    if (std::filesystem::exists(m_savedScenePath))
    {
        m_world->clear();
        SceneSerializer::loadScene(m_renderer, *m_world, m_savedScenePath);
        LOG_INFO("Scene state restored");
    }
}

