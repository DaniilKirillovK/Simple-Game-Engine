#include "Systems/RenderSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components/Transform.h"
#include "Components/MeshRenderer.h"
#include "Components/Camera.h"
#include "Entity.h"
#include "World.h"
#include "Components/Light.h"
#include <glm/gtc/type_ptr.hpp> 

RenderSystem::RenderSystem(IRenderAdapter* renderAdapter)
    : renderAdapter(renderAdapter)
{
}

void RenderSystem::update(World& world, float deltaTime)
{
    if (!renderAdapter) return;

    EntityId activeCamera = INVALID_ENTITY;
    Camera* cameraComponent = nullptr;
    Transform* cameraTransform = nullptr;

    auto cameras = world.getEntitiesWithComponent<Camera>();
    for (auto& [entity, camera] : cameras) 
    {
        if (camera->isActive)
        {
            activeCamera = entity;
            cameraComponent = camera;
            cameraTransform = world.getComponent<Transform>(entity);
            break;
        }
    }

    if (!cameraComponent || !cameraTransform) 
    {
        return;
    }

    glm::mat4 viewMatrix = cameraComponent->getViewMatrix(*cameraTransform);
    cameraComponent->aspectRatio = renderAdapter->getAspectRatio();
    glm::mat4 projectionMatrix = cameraComponent->getProjectionMatrix();

    std::vector<Light*> lights;
    auto lightEntities = world.getEntitiesWithComponent<Light>();
    for (auto& [entity, light] : lightEntities) 
    {
        if (light->enabled) 
        {
            lights.push_back(light);
        }
    }

    auto renderers = world.getEntitiesWithComponent<MeshRenderer>();

    for (auto& [entity, renderer] : renderers) 
    {
        if (!renderer->visible) return;
        if (!renderer->mesh || !renderer->material) return;

        Transform* transform = world.getComponent<Transform>(entity);
        if (!transform) return;

        glm::mat4 modelMatrix = transform->getLocalMatrix();
        glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
        glm::mat4 normalMatrix = glm::transpose(glm::inverse(modelMatrix));

        renderAdapter->setModelMatrix(glm::value_ptr(modelMatrix));
        renderAdapter->setViewMatrix(glm::value_ptr(viewMatrix));
        renderAdapter->setProjectionMatrix(glm::value_ptr(projectionMatrix));
        renderAdapter->setNormalMatrix(glm::value_ptr(normalMatrix));

        renderAdapter->setLights(lights);

        renderAdapter->setMaterial(renderer->material);

        renderAdapter->drawMesh(renderer->mesh);
    }
}
