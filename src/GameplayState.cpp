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

    //EntityId camera = world->createEntity();
    //world->addComponent<Transform>(camera, Transform{
    //    glm::vec3(0.0f, 5.0f, 10.0f),
    //    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
    //    glm::vec3(1.0f, 1.0f, 1.0f),
    //    glm::vec3(0.0f, 0.0f, 0.0f)
    //    });
    //world->addComponent<Tag>(camera, Tag{"Camera"});

    //Camera cam;
    //cam.aspectRatio = 16.f / 9.f;
    //cam.fov = 45.0f;
    //cam.nearPlane = 0.1f;
    //cam.farPlane = 100.0f;
    //cam.isActive = true;
    //world->addComponent<Camera>(camera, cam);

    LOG_INFO("3D scene setup complete");
}

