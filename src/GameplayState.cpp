#include "GameplayState.h"
#include "Logger.h"
#include "IRenderAdapter.h"

#include "Systems/RenderSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/TransformSystem.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "MeshFactory.h"
#include "Resources/ResourceManager.h"

#include "Components/Material.h"
#include "Components/Tag.h"
#include "Components/MeshRenderer.h"
#include "Components/Collider.h"
#include "Components/Rigidbody.h"
#include "Components/Transform.h"
#include "Components/Camera.h"
#include "Components/Light.h"

#include "Utils/HierarchyUtils.h"
#include <filesystem>
#include "Utils/Serialization/SceneSerializer.h"

void GameplayState::onEnter()
{
    world = new World();

    world->addSystem(std::make_unique<TransformSystem>());
    world->addSystem(std::make_unique<PhysicsSystem>(&renderer));
	world->addSystem(std::make_unique<RenderSystem>(&renderer));
	world->addSystem(std::make_unique<CameraSystem>());
	world->addSystem(std::make_unique<MovementSystem>());

	setupTestScene();

	LOG_INFO("Enter Gameplay State");
}

void GameplayState::update(float deltaTime)
{
	world->update(deltaTime);
}

void GameplayState::render()
{
	renderer.render(world);
}

void GameplayState::setupTestScene()
{
    std::string scenePath = "assets/scenes/test_scene.json";

    if (std::filesystem::exists(scenePath)) 
    {
        if (SceneSerializer::loadScene(renderer, *world, scenePath))
        {
            LOG_INFO("Scene loaded successfully from: " + scenePath);
        }
    }

    //// Loaded shaders with RESOURCE_MANAGER
    //auto vertShaderResource = RESOURCE_MANAGER.load<Shader>("Shaders/Default.vert");
    //auto fragShaderResource = RESOURCE_MANAGER.load<Shader>("Shaders/Default.frag");

    //Mesh* cubeMesh = MeshFactory::createCube();
    //Mesh* sphereMesh = MeshFactory::createSphere(0.5f, 36, 18);
    //Mesh* planeMesh = MeshFactory::createPlane(10.0f);

    //Material* redMaterial = new Material{ glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), vertShaderResource->get(), fragShaderResource->get(), renderer};
    //Material* greenMaterial = new Material{ glm::vec4(0.2f, 1.0f, 0.2f, 1.0f), vertShaderResource->get(), fragShaderResource->get(), renderer };
    //Material* blueMaterial = new Material{ glm::vec4(0.2f, 0.2f, 1.0f, 1.0f), vertShaderResource->get(), fragShaderResource->get(), renderer };
    //Material* yellowMaterial = new Material{ glm::vec4(1.0f, 1.0f, 0.2f, 1.0f), vertShaderResource->get(), fragShaderResource->get(), renderer };
    //Material* grayMaterial = new Material{ glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), vertShaderResource->get(), fragShaderResource->get(), renderer };
    //Material* purpleMaterial = new Material{ glm::vec4(0.6f, 0.2f, 0.8f, 1.0f), vertShaderResource->get(), fragShaderResource->get(), renderer };

    //// Load textures with RESOURCE_MANAGER
    //auto grassTextureResource = RESOURCE_MANAGER.load<Texture>("assets/t_grass.jpg");
    //EntityId grassPlatform = world->createEntity();
    //world->addComponent<Transform>(grassPlatform, Transform{
    //    glm::vec3(0.0f, 0.0f, 0.0f),
    //    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
    //    glm::vec3(20.0f, 1.0f, 20.0f),
    //    glm::vec3(0.0f, 0.0f, 0.0f)
    //    });
    //Texture* grassLoadedTexture = grassTextureResource->get();
    //Material* grassLoadedMaterial = new Material{ glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), vertShaderResource->get(), fragShaderResource->get(), renderer };
    //grassLoadedMaterial->diffuseTexture = grassLoadedTexture;
    //grassLoadedMaterial->hasTexture = true;
    //world->addComponent<MeshRenderer>(grassPlatform, MeshRenderer{ cubeMesh, grassLoadedMaterial });
    //world->addComponent<Rigidbody>(grassPlatform, Rigidbody{ 100.f, false, true });
    //world->addComponent<Collider>(grassPlatform, Collider{ glm::vec3(10.f, 0.5f, 10.f) });

    //EntityId wall1 = world->createEntity();
    //world->addComponent<Transform>(wall1, Transform{
    //    glm::vec3(-10.f, 2.5f, 0.0f),
    //    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
    //    glm::vec3(1.0f, 5.0f, 20.0f),
    //    glm::vec3(0.0f, 0.0f, 0.0f)
    //    });
    //world->addComponent<MeshRenderer>(wall1, MeshRenderer{ cubeMesh, grayMaterial });
    //world->addComponent<Rigidbody>(wall1, Rigidbody{ 1.0f, false, true });
    //world->addComponent<Collider>(wall1, Collider{ glm::vec3(0.5f, 2.5f, 10.f) });

    //EntityId wall2 = world->createEntity();
    //world->addComponent<Transform>(wall2, Transform{
    //    glm::vec3(0.f, 2.5f, 10.0f),
    //    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
    //    glm::vec3(20.0f, 5.0f, 1.0f),
    //    glm::vec3(0.0f, 0.0f, 0.0f)
    //    });
    //world->addComponent<MeshRenderer>(wall2, MeshRenderer{ cubeMesh, grayMaterial });
    //world->addComponent<Rigidbody>(wall2, Rigidbody{ 1.0f, false, true });
    //world->addComponent<Collider>(wall2, Collider{ glm::vec3(10.0f, 2.5f, 0.5f) });

    //EntityId wall3 = world->createEntity();
    //world->addComponent<Transform>(wall3, Transform{
    //    glm::vec3(10.f, 2.5f, 0.0f),
    //    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
    //    glm::vec3(1.0f, 5.0f, 20.0f),
    //    glm::vec3(0.0f, 0.0f, 0.0f)
    //    });
    //world->addComponent<MeshRenderer>(wall3, MeshRenderer{ cubeMesh, grayMaterial });
    //world->addComponent<Rigidbody>(wall3, Rigidbody{ 1.0f, false, true });
    //world->addComponent<Collider>(wall3, Collider{ glm::vec3(0.5f, 2.5f, 10.f) });

    //EntityId wall4 = world->createEntity();
    //world->addComponent<Transform>(wall4, Transform{
    //    glm::vec3(0.f, 2.5f, -10.0f),
    //    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
    //    glm::vec3(20.0f, 5.0f, 1.0f),
    //    glm::vec3(0.0f, 0.0f, 0.0f)
    //    });
    //world->addComponent<MeshRenderer>(wall4, MeshRenderer{ cubeMesh, grayMaterial });
    //world->addComponent<Rigidbody>(wall4, Rigidbody{ 1.0f, false, true });
    //world->addComponent<Collider>(wall4, Collider{ glm::vec3(10.f, 2.5f, 0.5f) });

    EntityId camera = world->createEntity();
    world->addComponent<Transform>(camera, Transform{
        glm::vec3(0.0f, 5.0f, 10.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f)
        });

    Camera cam;
    cam.aspectRatio = 16.f / 9.f;
    cam.fov = 45.0f;
    cam.nearPlane = 0.1f;
    cam.farPlane = 100.0f;
    cam.isActive = true;
    world->addComponent<Camera>(camera, cam);

    EntityId directionalLight = world->createEntity();
    Light dirLight;
    dirLight.type = LightType::Directional;
    dirLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
    dirLight.intensity = 0.5f;
    dirLight.direction = glm::vec3(0.57735f, -0.57735f, 0.57735f);
    dirLight.enabled = true;
    world->addComponent<Light>(directionalLight, dirLight);

    EntityId directionalLight2 = world->createEntity();
    Light dirLight2;
    dirLight2.type = LightType::Directional;
    dirLight2.color = glm::vec3(1.0f, 1.0f, 1.0f);
    dirLight2.intensity = 0.5f;
    dirLight2.direction = glm::vec3(-0.57735f, -0.57735f, 0.57735f);
    dirLight2.enabled = true;
    world->addComponent<Light>(directionalLight2, dirLight2);

    EntityId directionalLight3 = world->createEntity();
    Light dirLight3;
    dirLight3.type = LightType::Directional;
    dirLight3.color = glm::vec3(1.0f, 1.0f, 1.0f);
    dirLight3.intensity = 0.2f;
    dirLight3.direction = glm::vec3(0.0f, -0.707f, -0.707f);
    dirLight3.enabled = true;
    world->addComponent<Light>(directionalLight3, dirLight3);

    LOG_INFO("3D scene setup complete");
}

