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

GameplayState::GameplayState(IRenderAdapter& renderer)
: m_renderer(renderer)
{
    setupEditorCallbacks();
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
    m_world->addComponent<Transform>(newEntity, Transform{});
    m_world->addComponent<Tag>(newEntity, Tag{ "Empty" });
    m_renderer.setSelectedEntity(newEntity);
    LOG_INFO("Created empty entity: %d", newEntity);
}

void GameplayState::createCubeEntity()
{
    auto vertShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.vert");
    auto fragShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.frag");

    Mesh* cubeMesh = MeshFactory::createCube();

    Material* defaultMaterial = new Material{
        glm::vec4(0.8f, 0.8f, 0.8f, 1.0f),
        vertShader->get(),
        fragShader->get(),
        m_renderer
    };

    EntityId newEntity = m_world->createEntity();
    m_world->addComponent<Transform>(newEntity, Transform{});
    m_world->addComponent<MeshRenderer>(newEntity, MeshRenderer{ cubeMesh, defaultMaterial });
    m_world->addComponent<Tag>(newEntity, Tag{ "Cube" });
    m_world->addComponent<Rigidbody>(newEntity, Rigidbody{ 1.0f, true, false });
    m_world->addComponent<Collider>(newEntity, Collider{ glm::vec3(0.5f) });

    m_renderer.setSelectedEntity(newEntity);
    LOG_INFO("Created cube: %d", newEntity);
}

void GameplayState::createSphereEntity()
{
    auto vertShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.vert");
    auto fragShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.frag");

    Mesh* sphereMesh = MeshFactory::createSphere(0.5f, 36, 18);

    Material* defaultMaterial = new Material{
        glm::vec4(0.8f, 0.6f, 0.2f, 1.0f),
        vertShader->get(),
        fragShader->get(),
        m_renderer
    };

    EntityId newEntity = m_world->createEntity();
    m_world->addComponent<Transform>(newEntity, Transform{});
    m_world->addComponent<MeshRenderer>(newEntity, MeshRenderer{ sphereMesh, defaultMaterial });
    m_world->addComponent<Tag>(newEntity, Tag{ "Sphere" });
    m_world->addComponent<Rigidbody>(newEntity, Rigidbody{ 1.0f, true, false });
    m_world->addComponent<Collider>(newEntity, Collider{ 0.5f });

    m_renderer.setSelectedEntity(newEntity);
    LOG_INFO("Created sphere: %d", newEntity);
}

void GameplayState::createFromAsset(Mesh* mesh, const std::string& path)
{
    if (!mesh) return;

    auto vertShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.vert");
    auto fragShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.frag");

    Material* material = new Material{
        glm::vec4(0.8f, 0.8f, 0.8f, 1.0f),
        vertShader->get(),
        fragShader->get(),
        m_renderer
    };

    EntityId newEntity = m_world->createEntity();
    m_world->addComponent<Transform>(newEntity, Transform{});
    m_world->addComponent<MeshRenderer>(newEntity, MeshRenderer{ mesh, material });

    std::string name = std::filesystem::path(path).stem().string();
    m_world->addComponent<Tag>(newEntity, Tag{ name });

    m_renderer.setSelectedEntity(newEntity);
    LOG_INFO("Created object from model: %s", name.c_str());
}

