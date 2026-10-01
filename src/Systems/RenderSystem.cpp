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

#include "tracy/Tracy.hpp"

RenderSystem::RenderSystem(IRenderAdapter* renderAdapter)
    : m_renderAdapter(renderAdapter)
{
}

void RenderSystem::update(World& world, JobSystem* jobs, float deltaTime)
{
    ZoneScopedN("RenderSystem::update");

    if (!m_isEnabled || !m_renderAdapter) return;

    auto& camPool = world.getComponentPool<Camera>();
    auto& cams = camPool.components();
    auto& camEnts = camPool.entities();

    EntityId activeCamera = INVALID_ENTITY;
    Camera* cameraComponent = nullptr;

    for (size_t i = 0; i < cams.size(); ++i)
    {
        if (cams[i].isActive)
        {
            activeCamera = camEnts[i];
            cameraComponent = &cams[i];
            break;
        }
    }

    if (!cameraComponent) return;

    auto& tPool = world.getComponentPool<Transform>();
    Transform* cameraTransform = tPool.getComponent(activeCamera);
    if (!cameraTransform) return;

    const glm::mat4 viewMatrix = cameraComponent->getViewMatrix(*cameraTransform);
    cameraComponent->aspectRatio = m_renderAdapter->getAspectRatio();
    const glm::mat4 projectionMatrix = cameraComponent->getProjectionMatrix();

    const float* viewPtr = glm::value_ptr(viewMatrix);
    const float* projPtr = glm::value_ptr(projectionMatrix);

    m_lights.clear();

    auto& lightPool = world.getComponentPool<Light>();
    auto& lightComps = lightPool.components();

    for (auto& light : lightComps)
    {
        if (light.enabled)
            m_lights.push_back(&light);
    }

    auto& mrPool = world.getComponentPool<MeshRenderer>();
    auto& renderers = mrPool.components();
    auto& rendererEnts = mrPool.entities();

    for (size_t i = 0; i < renderers.size(); ++i)
    {
        auto& r = renderers[i];
        if (!r.visible || !r.mesh || !r.material) continue;

        if (!r.mesh->ready) continue;

        Transform* tr = tPool.getComponent(rendererEnts[i]);
        if (!tr) continue;

        const glm::mat4& modelMatrix = tr->worldMatrix;
        const glm::mat4 normalMatrix = glm::transpose(glm::inverse(modelMatrix));

        m_renderAdapter->setShaderProgram(r.material->shaderProgram);

        m_renderAdapter->setModelMatrix(glm::value_ptr(modelMatrix));
        m_renderAdapter->setViewMatrix(viewPtr);
        m_renderAdapter->setProjectionMatrix(projPtr);
        m_renderAdapter->setNormalMatrix(glm::value_ptr(normalMatrix));

        m_renderAdapter->setLights(m_lights);
        m_renderAdapter->setMaterial(r.material);
        m_renderAdapter->drawMesh(r.mesh);
    }
}

void RenderSystem::setEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}
