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
#include "Resources/ResourceManager.h"
#include "MeshFactory.h"
#include "Components/MeshRenderer.h"

#include "JobSystem/JobSystem.h"

#include "tracy/Tracy.hpp"

GameplayState::GameplayState(IRenderAdapter& renderer)
: m_renderer(renderer)
{
    setupEditorCallbacks();
}

void GameplayState::onEnter()
{
    m_world = new World();

    m_world->addSystem(std::make_unique<PhysicsSystem>(&m_renderer));
    m_world->addSystem(std::make_unique<CameraSystem>());
    m_world->addSystem(std::make_unique<MovementSystem>());
    m_world->addSystem(std::make_unique<TransformSystem>());
    m_world->addSystem(std::make_unique<RenderSystem>(&m_renderer));

	setupTestScene();

	LOG_INFO("Enter Gameplay State");
}

void GameplayState::update(JobSystem* jobs, float deltaTime)
{
    ZoneScoped;

    m_world->update(jobs, deltaTime);
}

void GameplayState::render()
{
    m_renderer.render(m_world);
}

void GameplayState::setupEditorCallbacks()
{
    m_renderer.setOnPlayCallback([this]() {
        enterPlayMode();
        });

    m_renderer.setOnStopCallback([this]() {
        exitPlayMode();
        });

    m_renderer.setOnCreateEmptyCallback([this]() {
        createEmptyEntity();
        });

    m_renderer.setOnCreateCubeCallback([this]() {
        createCubeEntity();
        });

    m_renderer.setOnCreateSphereCallback([this]() {
        createSphereEntity();
        });

    m_renderer.setOnCreateFromAssetCallback([this](Mesh* mesh, const std::string& path) {
        createFromAsset(mesh, path);
        });
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

void GameplayState::createEmptyEntity()
{
    EntityId newEntity = m_world->createEntity();
    m_world->getComponentPool<Transform>().addComponent(newEntity, Transform{});
    m_world->getComponentPool<Tag>().addComponent(newEntity, Tag{ "Empty" });
    m_renderer.setSelectedEntity(newEntity);
    LOG_INFO("Created empty entity: %d", newEntity);
}

void GameplayState::createCubeEntity()
{
    createMeshEntity(MeshFactory::createCube(), "Cube",
        glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), true);
}

void GameplayState::createSphereEntity()
{
    createMeshEntity(MeshFactory::createSphere(0.5f, 36, 18), "Sphere",
        glm::vec4(0.8f, 0.6f, 0.2f, 1.0f), true);
}

void GameplayState::createFromAsset(Mesh* mesh, const std::string& path)
{
    if (!mesh) return;
    std::string name = std::filesystem::path(path).stem().string();
    createMeshEntity(mesh, name, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), false);
}

void GameplayState::createMeshEntity(Mesh* mesh, const std::string& tagName, const glm::vec4& color, bool withPhysics)
{
    if (!mesh) return;

    auto vertShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.vert");
    auto fragShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.frag");

    if (!vertShader || !fragShader) return;

    Material* material = new Material{
        color,
        vertShader->get(),
        fragShader->get(),
        m_renderer
    };

    const EntityId e = m_world->createEntity();

    auto& tPool = m_world->getComponentPool<Transform>();
    auto& mrPool = m_world->getComponentPool<MeshRenderer>();
    auto& tagPool = m_world->getComponentPool<Tag>();
    auto& rbPool = m_world->getComponentPool<Rigidbody>();
    auto& cPool = m_world->getComponentPool<Collider>();

    tPool.addComponent(e, Transform{});
    mrPool.addComponent(e, MeshRenderer{ mesh, material });
    tagPool.addComponent(e, Tag{ tagName });

    if (withPhysics)
    {
        rbPool.addComponent(e, Rigidbody{ 1.0f, true, false });
        cPool.addComponent(e, Collider{ 0.5f });
    }

    m_renderer.setSelectedEntity(e);
    LOG_INFO("Created entity: " + std::to_string(e) + " (" + tagName + ")");
}

