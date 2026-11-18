#include "Systems/RenderSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <string>
#include <cstring>
#include "Components/Transform.h"
#include "Components/MeshRenderer.h"
#include "Components/Camera.h"
#include "Entity.h"
#include "World.h"
#include "Components/Light.h"
#include "Scripting/ScriptGilWindow.h"
#include <glm/gtc/type_ptr.hpp>

#include "tracy/Tracy.hpp"

namespace
{
    // Bitwise ordering helper: deterministic total order over raw float bytes.
    template <size_t N>
    struct FloatBytes
    {
        float v[N];
    };

    template <size_t N>
    bool operator<(const FloatBytes<N>& a, const FloatBytes<N>& b)
    {
        int cmp = std::memcmp(a.v, b.v, sizeof(a.v));
        if (cmp) return cmp < 0;
        return false;
    }

    // Identity of a mesh by data: two entities with separate but equal meshes batch together.
    struct MeshKey
    {
        MeshType type = MeshType::Cube;
        std::string path;
        size_t vertexCount = 0;
        size_t indexCount = 0;
        FloatBytes<3> first;
        FloatBytes<3> last;
    };

    bool operator<(const MeshKey& a, const MeshKey& b)
    {
        if (a.type != b.type) return int(a.type) < int(b.type);
        if (a.path != b.path) return a.path < b.path;
        if (a.vertexCount != b.vertexCount) return a.vertexCount < b.vertexCount;
        if (a.indexCount != b.indexCount) return a.indexCount < b.indexCount;
        if (a.first < b.first) return true;
        if (b.first < a.first) return false;
        return a.last < b.last;
    }

    // Identity of a material by data: same shaders, texture, colors and shininess batch together.
    struct MaterialKey
    {
        uint32_t vertexShader = 0;
        uint32_t pixelShader = 0;
        std::string vsPath;
        std::string fsPath;
        uint32_t texture = 0;
        bool hasTexture = false;
        FloatBytes<4> diffuse;
        FloatBytes<4> specular;
        float shininess = 0.0f;
    };

    bool operator<(const MaterialKey& a, const MaterialKey& b)
    {
        if (a.vertexShader != b.vertexShader) return a.vertexShader < b.vertexShader;
        if (a.pixelShader != b.pixelShader) return a.pixelShader < b.pixelShader;
        if (a.vsPath != b.vsPath) return a.vsPath < b.vsPath;
        if (a.fsPath != b.fsPath) return a.fsPath < b.fsPath;
        if (a.texture != b.texture) return a.texture < b.texture;
        if (a.hasTexture != b.hasTexture) return a.hasTexture < b.hasTexture;
        if (a.diffuse < b.diffuse) return true;
        if (b.diffuse < a.diffuse) return false;
        if (a.specular < b.specular) return true;
        if (b.specular < a.specular) return false;
        return std::memcmp(&a.shininess, &b.shininess, sizeof(float)) < 0;
    }

    struct BatchKey
    {
        MeshKey mesh;
        MaterialKey material;
    };

    bool operator<(const BatchKey& a, const BatchKey& b)
    {
        if (a.mesh < b.mesh) return true;
        if (b.mesh < a.mesh) return false;
        return a.material < b.material;
    }

    struct Batch
    {
        const Mesh* mesh = nullptr;         // representative: drawn once for the whole batch
        const Material* material = nullptr;
        std::vector<InstanceData> instances;
    };
}

RenderSystem::RenderSystem(IRenderAdapter* renderAdapter)
    : m_renderAdapter(renderAdapter)
{
}

void RenderSystem::update(World& world, float deltaTime)
{
    ZoneScopedN("RenderSystem::update");

    ScriptGilWindow gilWindow;

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

    // The camera matrices are per-frame state, shared by every submission path
    // (per-object draws AND batched instanced draws).
    m_renderAdapter->setViewMatrix(viewPtr);
    m_renderAdapter->setProjectionMatrix(projPtr);

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

    // Submits one renderer through the classic per-object path.
    auto drawSingle = [&](MeshRenderer& r, EntityId entity)
    {
        Transform* tr = tPool.getComponent(entity);
        if (!tr) return;

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
    };

    // A renderer can be instanced only when it uses the instancing-aware vertex shader.
    auto instancingCompatible = [](const MeshRenderer& r)
    {
        return r.material && r.material->shaderProgram.vertex && r.material->shaderProgram.pixel
            && r.material->vertexShaderPath.rfind("Default.vert") != std::string::npos;
    };

    if (m_renderAdapter->isInstancingEnabled())
    {
        // Batch renderers by (mesh data, material data): identical mesh + material = one draw call.
        std::map<BatchKey, Batch> batches;
        // Draw order must match the per-object path exactly (depth ties at coplanar
        // surfaces break by draw order), so batches are flushed in the order of
        // their first occurrence in the entity iteration.
        std::map<size_t, BatchKey> batchOrder;

        for (size_t i = 0; i < renderers.size(); ++i)
        {
            auto& r = renderers[i];
            if (!r.visible || !r.mesh || !r.material) continue;
            if (!r.mesh->ready || r.mesh->vertices.empty()) continue;

            if (!instancingCompatible(r))
            {
                drawSingle(r, rendererEnts[i]);
                continue;
            }

            Transform* tr = tPool.getComponent(rendererEnts[i]);
            if (!tr) continue;

            BatchKey key;
            key.mesh.type = r.mesh->type;
            key.mesh.path = r.mesh->path;
            key.mesh.vertexCount = r.mesh->vertices.size();
            key.mesh.indexCount = r.mesh->indices.size();
            const glm::vec3& first = r.mesh->vertices.front();
            const glm::vec3& last = r.mesh->vertices.back();
            key.mesh.first.v[0] = first.x; key.mesh.first.v[1] = first.y; key.mesh.first.v[2] = first.z;
            key.mesh.last.v[0] = last.x; key.mesh.last.v[1] = last.y; key.mesh.last.v[2] = last.z;

            key.material.vertexShader = r.material->shaderProgram.vertex;
            key.material.pixelShader = r.material->shaderProgram.pixel;
            key.material.vsPath = r.material->vertexShaderPath;
            key.material.fsPath = r.material->fragmentShaderPath;
            key.material.hasTexture = r.material->hasTexture;
            key.material.texture = r.material->diffuseTexture ? r.material->diffuseTexture->m_handle : 0;
            const glm::vec4& diffuse = r.material->diffuseColor;
            key.material.diffuse.v[0] = diffuse.r; key.material.diffuse.v[1] = diffuse.g;
            key.material.diffuse.v[2] = diffuse.b; key.material.diffuse.v[3] = diffuse.a;
            const glm::vec4& specular = r.material->specularColor;
            key.material.specular.v[0] = specular.r; key.material.specular.v[1] = specular.g;
            key.material.specular.v[2] = specular.b; key.material.specular.v[3] = specular.a;
            key.material.shininess = r.material->shininess;

            Batch& batch = batches[key];
            if (!batch.mesh)
            {
                batch.mesh = r.mesh.get();
                batch.material = r.material.get();
                batchOrder[i] = key;
            }

            InstanceData instance;
            instance.model = tr->worldMatrix;
            instance.normal = glm::transpose(glm::inverse(tr->worldMatrix));
            batch.instances.push_back(instance);
        }

        for (auto& [index, key] : batchOrder)
        {
            Batch& batch = batches[key];
            m_renderAdapter->setShaderProgram(&batch.material->shaderProgram);
            m_renderAdapter->setLights(m_lights);
            m_renderAdapter->setMaterial(batch.material);
            m_renderAdapter->drawMeshInstanced(batch.mesh, batch.instances, m_renderAdapter->isIndirectDrawEnabled());
        }
    }
    else
    {
        for (size_t i = 0; i < renderers.size(); ++i)
        {
            auto& r = renderers[i];
            if (!r.visible || !r.mesh || !r.material) continue;
            if (!r.mesh->ready) continue;

            drawSingle(r, rendererEnts[i]);
        }
    }
}

void RenderSystem::setEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}
