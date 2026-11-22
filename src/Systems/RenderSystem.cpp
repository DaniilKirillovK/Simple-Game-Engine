#include "Systems/RenderSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components/Transform.h"
#include "Components/MeshRenderer.h"
#include "Components/Camera.h"
#include "Entity.h"
#include "World.h"
#include "Components/Light.h"
#include "Scripting/ScriptGilWindow.h"
#include <glm/gtc/type_ptr.hpp>
#include "Utils/EntityBounds.h"
#include "Utils/Frustum.h"
#include <chrono>

#include "tracy/Tracy.hpp"

RenderSystem::RenderSystem(IRenderAdapter* renderAdapter)
    : m_renderAdapter(renderAdapter)
{
}

void RenderSystem::update(World& world, float)
{
    ZoneScopedN("RenderSystem::update");

    ScriptGilWindow gilWindow;

    m_statistics = {};
    m_visibleRenderers.clear();

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

    m_statistics.totalRenderers = renderers.size();
    const auto visibilityStart = std::chrono::steady_clock::now();
    {
        ZoneScopedN("CPU frustum culling / visible set");
        m_visibleRenderers.reserve(renderers.size());
        const Frustum frustum(projectionMatrix * viewMatrix);
        for (size_t i = 0; i < renderers.size(); ++i)
        {
            const auto& r = renderers[i];
            if (!r.visible || !r.mesh || !r.material || !r.mesh->ready) continue;

            const Transform* tr = tPool.getComponent(rendererEnts[i]);
            if (!tr) continue;
            ++m_statistics.candidates;

            if (m_frustumCullingEnabled && !r.mesh->vertices.empty())
            {
                const Bounds bounds = EntityBounds::meshWorldBounds(*r.mesh, tr->worldMatrix);
                ++m_statistics.boundsTested;
                if (!frustum.intersects(bounds.min, bounds.max))
                {
                    ++m_statistics.culled;
                    continue;
                }
            }
            m_visibleRenderers.push_back(i);
        }
    }
    m_statistics.visibilityCpuMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - visibilityStart).count();

    const auto submissionStart = std::chrono::steady_clock::now();
    {
        ZoneScopedN("Render submission");
        for (size_t i : m_visibleRenderers)
        {
            auto& r = renderers[i];
            const Transform* tr = tPool.getComponent(rendererEnts[i]);

            const glm::mat4& modelMatrix = tr->worldMatrix;
            const glm::mat4 normalMatrix = glm::transpose(glm::inverse(modelMatrix));

            m_renderAdapter->setShaderProgram(&r.material->shaderProgram);

            m_renderAdapter->setModelMatrix(glm::value_ptr(modelMatrix));
            m_renderAdapter->setViewMatrix(viewPtr);
            m_renderAdapter->setProjectionMatrix(projPtr);
            m_renderAdapter->setNormalMatrix(glm::value_ptr(normalMatrix));

            m_renderAdapter->setLights(m_lights);
            m_renderAdapter->setMaterial(r.material.get());
            m_renderAdapter->drawMesh(r.mesh.get());
            ++m_statistics.submitted;
        }
    }
    m_statistics.submissionCpuMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - submissionStart).count();
}

void RenderSystem::setEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}
