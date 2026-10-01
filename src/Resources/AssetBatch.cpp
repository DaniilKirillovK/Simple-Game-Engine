#include "Resources/AssetBatch.h"

#include "Resources/ResourceManager.h"
#include "Resources/MeshLoader/MeshLoader.h"
#include "Resources/TextureLoader/TextureLoader.h"
#include "Resources/TextureLoader/Texture.h"
#include "Components/MeshRenderer.h"
#include "Logger.h"

#include "tracy/Tracy.hpp"

#include <algorithm>

// CPU stage: mesh decoding (Assimp)
struct MeshLoadCtx
{
    const std::vector<std::string>* paths;
    std::vector<std::unique_ptr<Mesh>>* out;
};

void loadMeshJob(void* raw, uint32_t i)
{
    auto* ctx = static_cast<MeshLoadCtx*>(raw);
    (*ctx->out)[i] = MeshLoader::getInstance().loadMesh((*ctx->paths)[i]);
}

//CPU stage: texture decoding (stb_image)

struct TextureLoadCtx
{
    const std::vector<std::string>* paths;
    std::vector<std::unique_ptr<TextureData>>* out;
};

void loadTextureJob(void* raw, uint32_t i)
{
    auto* ctx = static_cast<TextureLoadCtx*>(raw);
    (*ctx->out)[i] = TextureLoader::getInstance().loadFromFile((*ctx->paths)[i]);
}

AssetBatch::AssetBatch(JobSystem* jobs, IRenderAdapter* renderAdapter)
    : m_jobs(jobs)
    , m_renderAdapter(renderAdapter)
{
}

void AssetBatch::requestMesh(const std::string& path)
{
    if (path.empty()) return;
    if (!m_requestedMeshes.insert(path).second) return; // already requested

    // Nothing to do if the resource manager already has it.
    if (RESOURCE_MANAGER.isLoaded<Mesh>(path)) return;

    m_meshPaths.push_back(path);
}

void AssetBatch::requestTexture(const std::string& path)
{
    if (path.empty()) return;
    if (!m_requestedTextures.insert(path).second) return;

    if (RESOURCE_MANAGER.isLoaded<Texture>(path)) return;

    m_texturePaths.push_back(path);
}

void AssetBatch::flush()
{
    ZoneScopedN("AssetBatch::flush");

    // Meshes: parallel decode, then register
    if (!m_meshPaths.empty())
    {
        ZoneScopedN("AssetBatch::Meshes");

        const uint32_t count = static_cast<uint32_t>(m_meshPaths.size());
        m_meshResults.assign(count, nullptr);

        MeshLoadCtx ctx{ &m_meshPaths, &m_meshResults };

        if (m_jobs && count > 1)
        {
            // One asset per job: asset decoding is coarse-grained enough that
            // per-batch splitting would only add imbalance.
            m_jobs->parallelFor(count, 1, loadMeshJob, &ctx);
        }
        else
        {
            for (uint32_t i = 0; i < count; ++i) loadMeshJob(&ctx, i);
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            if (!m_meshResults[i]) continue;

            auto resource = RESOURCE_MANAGER.insert<Mesh>(m_meshPaths[i], std::move(m_meshResults[i]));
            if (resource && resource->isValid())
            {
                m_meshCache[m_meshPaths[i]] = resource->get();
            }
        }

        LOG_INFO("AssetBatch: loaded " + std::to_string(m_meshCache.size()) +
                 " mesh(es) in parallel");
    }

    //  Textures: parallel decode + serial GPU upload
    if (!m_texturePaths.empty())
    {
        ZoneScopedN("AssetBatch::Textures");

        const uint32_t count = static_cast<uint32_t>(m_texturePaths.size());
        m_textureDataResults.assign(count, nullptr);

        TextureLoadCtx ctx{ &m_texturePaths, &m_textureDataResults };

        if (m_jobs && count > 1)
        {
            m_jobs->parallelFor(count, 1, loadTextureJob, &ctx);
        }
        else
        {
            for (uint32_t i = 0; i < count; ++i) loadTextureJob(&ctx, i);
        }

        // The GPU upload must happen on the GL thread, so it stays serial.
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!m_textureDataResults[i]) continue;

            auto texture = TextureLoader::getInstance().createGPU(*m_textureDataResults[i], m_texturePaths[i]);
            m_textureDataResults[i].reset();

            if (!texture) continue;

            auto resource = RESOURCE_MANAGER.insert<Texture>(m_texturePaths[i], std::move(texture));
            if (resource && resource->isValid())
            {
                m_textureCache[m_texturePaths[i]] = resource->get();
            }
        }

        LOG_INFO("AssetBatch: uploaded " + std::to_string(m_textureCache.size()) +
                 " texture(s) to the GPU");
    }
}

Mesh* AssetBatch::getMesh(const std::string& path) const
{
    auto it = m_meshCache.find(path);
    if (it != m_meshCache.end()) return it->second;

    auto resource = RESOURCE_MANAGER.get<Mesh>(path);
    return resource ? resource->get() : nullptr;
}

Texture* AssetBatch::getTexture(const std::string& path) const
{
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) return it->second;

    auto resource = RESOURCE_MANAGER.get<Texture>(path);
    return resource ? resource->get() : nullptr;
}
