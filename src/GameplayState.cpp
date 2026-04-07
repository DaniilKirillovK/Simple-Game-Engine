#include "GameplayState.h"
#include "Logger.h"
#include "IRenderAdapter.h"

#include "Systems/RenderSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/CameraSystem.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "MeshFactory.h"

#include "Components/Material.h"
#include "Components/Tag.h"
#include "Components/MeshRenderer.h"
#include "Components/Transform.h"
#include "Components/Camera.h"
#include "Components/Light.h"

void GameplayState::onEnter()
{
	world.addSystem(std::make_unique<RenderSystem>(&renderer));
	world.addSystem(std::make_unique<CameraSystem>(&input));
	world.addSystem(std::make_unique<MovementSystem>());

	setupTestScene();

	LOG_INFO("Enter Gameplay State");
}

void GameplayState::update(float deltaTime)
{
	world.update(deltaTime);
}

void GameplayState::render()
{
	renderer.render();
}

void GameplayState::setupTestScene()
{
    Mesh* cubeMesh = MeshFactory::createCube();
    Mesh* sphereMesh = MeshFactory::createSphere(0.5f, 36, 18);
    Mesh* planeMesh = MeshFactory::createPlane(10.0f);

    Material* redMaterial = new Material{ glm::vec4(1.0f, 0.2f, 0.2f, 1.0f) };
    Material* greenMaterial = new Material{ glm::vec4(0.2f, 1.0f, 0.2f, 1.0f) };
    Material* blueMaterial = new Material{ glm::vec4(0.2f, 0.2f, 1.0f, 1.0f) };
    Material* yellowMaterial = new Material{ glm::vec4(1.0f, 1.0f, 0.2f, 1.0f) };
    Material* grayMaterial = new Material{ glm::vec4(0.5f, 0.5f, 0.5f, 1.0f) };

    EntityId redCube = world.createEntity();
    world.addComponent<Transform>(redCube, Transform{
        glm::vec3(-3.0f, 0.5f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 45.0f, 0.0f)
        });
    world.addComponent<MeshRenderer>(redCube, MeshRenderer{ cubeMesh, redMaterial });
    world.addComponent<Tag>(redCube, Tag{ "Rotating" });

    EntityId greenCube = world.createEntity();
    world.addComponent<Transform>(greenCube, Transform{
        glm::vec3(3.0f, 0.5f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f)
        });
    world.addComponent<MeshRenderer>(greenCube, MeshRenderer{ cubeMesh, greenMaterial });
    world.addComponent<Tag>(greenCube, Tag{ "Bouncing" });

    EntityId yellowCube = world.createEntity();
    world.addComponent<Transform>(yellowCube, Transform{
        glm::vec3(0.0f, 0.5f, -3.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f)
        });
    world.addComponent<MeshRenderer>(yellowCube, MeshRenderer{ cubeMesh, yellowMaterial });
    world.addComponent<Tag>(yellowCube, Tag{ "Static" });

    EntityId camera = world.createEntity();
    world.addComponent<Transform>(camera, Transform{
        glm::vec3(0.0f, 5.0f, 10.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f)
        });

    Camera cam;
    cam.aspectRatio = 800.0f / 600.0f;
    cam.fov = 45.0f;
    cam.nearPlane = 0.1f;
    cam.farPlane = 100.0f;
    cam.isActive = true;
    world.addComponent<Camera>(camera, cam);

    EntityId directionalLight = world.createEntity();
    Light dirLight;
    dirLight.type = LightType::Directional;
    dirLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
    dirLight.intensity = 0.6f;
    dirLight.direction = glm::vec3(0.57735f, -0.57735f, 0.57735f);
    dirLight.enabled = true;
    world.addComponent<Light>(directionalLight, dirLight);

    EntityId directionalLight2 = world.createEntity();
    Light dirLight2;
    dirLight2.type = LightType::Directional;
    dirLight2.color = glm::vec3(1.0f, 1.0f, 1.0f);
    dirLight2.intensity = 0.3;
    dirLight2.direction = glm::vec3(-0.57735f, -0.57735f, 0.57735f);
    dirLight2.enabled = true;
    world.addComponent<Light>(directionalLight2, dirLight2);

    EntityId directionalLight3 = world.createEntity();
    Light dirLight3;
    dirLight3.type = LightType::Directional;
    dirLight3.color = glm::vec3(1.0f, 1.0f, 1.0f);
    dirLight3.intensity = 0.15f;
    dirLight3.direction = glm::vec3(0.0f, -0.707f, -0.707f);
    dirLight3.enabled = true;
    world.addComponent<Light>(directionalLight3, dirLight3);

    LOG_INFO("3D scene setup complete");
}

