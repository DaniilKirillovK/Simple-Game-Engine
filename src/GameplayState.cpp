#include "GameplayState.h"
#include "Logger.h"
#include "IRenderAdapter.h"

#include "Systems/RenderSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/TransformSystem.h"
#include "Systems/ScriptSystem.h"

#include <filesystem>
#include "Utils/Serialization/SceneSerializer.h"
#include "Resources/ResourceManager.h"
#include "MeshFactory.h"
#include "Components/MeshRenderer.h"

#include "JobSystem/JobSystem.h"

#include "tracy/Tracy.hpp"

GameplayState::GameplayState(IRenderAdapter& renderer)
: m_renderer(renderer),
  m_prefabs(std::make_unique<PrefabLibrary>(renderer))
{
    setupEditorCallbacks();
}

void GameplayState::onEnter()
{
    ZoneScopedN("GameplayState::onEnter");

    m_world = new World();

    m_world->addSystem(std::make_unique<PhysicsSystem>(&m_renderer));
    m_world->addSystem(std::make_unique<CameraSystem>());
    m_world->addSystem(std::make_unique<MovementSystem>());
    m_world->addSystem(std::make_unique<ScriptSystem>(
        [this](const std::string& primitive, const glm::vec3& position) {
            return spawnPrimitive(primitive, position);
        },
        [this](const std::string& prefab, const glm::vec3& position, const nlohmann::json& fieldOverrides) {
            return m_prefabs->instantiate(*m_world, prefab, position, fieldOverrides);
        }));
    m_world->addSystem(std::make_unique<TransformSystem>());
    m_world->addSystem(std::make_unique<RenderSystem>(&m_renderer));

	setupTestScene();

	LOG_INFO("Enter Gameplay State");
}

void GameplayState::onExit()
{
    if (ScriptSystem* scriptSystem = m_world->getSystem<ScriptSystem>())
    {
        scriptSystem->stop();
    }
}

void GameplayState::update(float deltaTime)
{
    ZoneScoped;

    m_world->update(deltaTime);
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

    m_renderer.setPrefabLibrary(m_prefabs.get());
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

    if (ScriptSystem* scriptSystem = m_world->getSystem<ScriptSystem>())
    {
        scriptSystem->start(*m_world);
    }
}

void GameplayState::exitPlayMode()
{
    LOG_INFO("Exiting PLAY mode, restoring scene state");

    if (ScriptSystem* scriptSystem = m_world->getSystem<ScriptSystem>())
    {
        scriptSystem->stop();
    }

    if (PhysicsSystem* physicsSystem = m_world->getSystem<PhysicsSystem>())
    {
        physicsSystem->setEnabled(false);
    }

    m_playModeActive = false;
    restoreSceneState();
}

void GameplayState::setupTestScene()
{
    std::string scenePath = "assets\\scenes\\test_scene.json";

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
    m_savedScenePath = "assets\\scenes\\temp_save.json";
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
    LOG_INFO("Created empty entity: " + std::to_string(newEntity));
}

void GameplayState::createCubeEntity()
{
    EntityId e = createMeshEntity(MeshFactory::createCube(), "Cube",
        glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), true);
    if (e != INVALID_ENTITY) m_renderer.setSelectedEntity(e);
}

void GameplayState::createSphereEntity()
{
    EntityId e = createMeshEntity(MeshFactory::createSphere(0.5f, 36, 18), "Sphere",
        glm::vec4(0.8f, 0.6f, 0.2f, 1.0f), true);
    if (e != INVALID_ENTITY) m_renderer.setSelectedEntity(e);
}

void GameplayState::createFromAsset(Mesh* mesh, const std::string& path)
{
    if (!mesh) return;
    std::string name = std::filesystem::path(path).stem().string();
    EntityId e = createMeshEntity(mesh, name, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), false);
    if (e != INVALID_ENTITY) m_renderer.setSelectedEntity(e);
}

EntityId GameplayState::createMeshEntity(Mesh* mesh, const std::string& tagName, const glm::vec4& color, bool withPhysics)
{
    if (!mesh) return INVALID_ENTITY;

    auto vertShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.vert");
    auto fragShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.frag");

    if (!vertShader || !fragShader) return INVALID_ENTITY;

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

    LOG_INFO("Created entity: " + std::to_string(e) + " (" + tagName + ")");
    return e;
}

EntityId GameplayState::spawnPrimitive(const std::string& primitive, const glm::vec3& position)
{
    EntityId e = INVALID_ENTITY;
    if (primitive == "cube")
    {
        e = createMeshEntity(MeshFactory::createCube(), "Cube",
            glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), true);
    }
    else if (primitive == "sphere")
    {
        e = createMeshEntity(MeshFactory::createSphere(0.5f, 36, 18), "Sphere",
            glm::vec4(0.8f, 0.6f, 0.2f, 1.0f), true);
    }

    if (Transform* transform = m_world->getComponent<Transform>(e))
    {
        transform->position = position;
    }
    return e;
}

