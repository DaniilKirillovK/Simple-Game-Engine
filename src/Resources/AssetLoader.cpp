#include "Resources/AssetLoader.h"

#include "IRenderAdapter.h"
#include "Logger.h"
#include "Components/MeshRenderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/MeshLoader/MeshLoader.h"
#include "Resources/TextureLoader/Texture.h"
#include "Resources/TextureLoader/TextureLoader.h"

#include "tracy/Tracy.hpp"

#include <chrono>
#include <thread>

AssetLoader::AssetLoader() = default;
AssetLoader::~AssetLoader() = default;

void AssetLoader::initialize(IRenderAdapter* renderer)
{
    m_renderer = renderer;
    m_stopping.store(false);

    unsigned char white[4] = { 255, 255, 255, 255 };
    TextureData whitePixel(1, 1, 4, white);
    m_placeholderTexture = m_renderer->createTexture(whitePixel);
    whitePixel.pixels = nullptr;

    LOG_INFO("AssetLoader initialized");
}

void AssetLoader::shutdown()
{
    ZoneScopedN("Asset::Shutdown");

    m_stopping.store(true);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (m_inFlight.load() > 0)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            LOG_ERROR("AssetLoader: shutdown timed out with decode jobs still running");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_ready.clear();
    }
    m_pending.store(0);

    LOG_INFO("AssetLoader shut down");
}

std::shared_ptr<Resource<Mesh>> AssetLoader::requestMesh(const std::string& path)
{
    ZoneScopedN("Asset::RequestMesh");
    ZoneText(path.c_str(), path.size());

    if (path.empty() || m_stopping.load())
    {
        return nullptr;
    }

    if (auto existing = RESOURCE_MANAGER.get<Mesh>(path))
    {
        return existing;
    }

    auto placeholder = std::make_unique<Mesh>();
    placeholder->type = MeshType::Custom;
    placeholder->path = path;
    placeholder->ready = false;

    auto resource = RESOURCE_MANAGER.insert<Mesh>(path, std::move(placeholder));
    resource->setState(ResourceState::Loading);

    auto request = std::make_unique<Request>();
    request->isMesh = true;
    request->path = path;
    request->meshResource = resource;
    dispatch(std::move(request));

    return resource;
}

std::shared_ptr<Resource<Texture>> AssetLoader::requestTexture(const std::string& path)
{
    ZoneScopedN("Asset::RequestTexture");
    ZoneText(path.c_str(), path.size());

    if (path.empty() || m_stopping.load())
    {
        return nullptr;
    }

    if (auto existing = RESOURCE_MANAGER.get<Texture>(path))
    {
        return existing;
    }

    auto placeholder = std::make_unique<Texture>();
    placeholder->m_handle = m_placeholderTexture;
    placeholder->m_width = 1;
    placeholder->m_height = 1;
    placeholder->m_channels = 4;
    placeholder->m_path = path;
    placeholder->ready = false;

    auto resource = RESOURCE_MANAGER.insert<Texture>(path, std::move(placeholder));
    resource->setState(ResourceState::Loading);

    auto request = std::make_unique<Request>();
    request->isMesh = false;
    request->path = path;
    request->textureResource = resource;
    dispatch(std::move(request));

    return resource;
}

void AssetLoader::dispatch(std::unique_ptr<Request> request)
{
    m_pending.fetch_add(1);
    m_inFlight.fetch_add(1);

    JobSystem::get().execute(&AssetLoader::decodeJob, request.release());
}

void AssetLoader::decodeJob(void* raw, uint32_t /*index*/)
{
    std::unique_ptr<Request> request(static_cast<Request*>(raw));
    AssetLoader& self = AssetLoader::getInstance();

    if (!self.m_stopping.load())
    {
        if (request->isMesh)
        {
            ZoneScopedN("Asset::DecodeMesh");
            ZoneText(request->path.c_str(), request->path.size());

            request->meshData = MeshLoader::getInstance().loadMesh(request->path);
        }
        else
        {
            ZoneScopedN("Asset::DecodeTexture");
            ZoneText(request->path.c_str(), request->path.size()); 

            request->textureData = TextureLoader::getInstance().loadFromFile(request->path);
        }
    }

    self.complete(request.release());
}

void AssetLoader::complete(Request* request)
{
    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_ready.emplace_back(request);
    }

    m_inFlight.fetch_sub(1);
}

void AssetLoader::pump(float budgetMs)
{
    ZoneScopedN("Asset::Pump");
    TracyPlot("Asset::Pending", static_cast<int64_t>(m_pending.load()));

    const auto start = std::chrono::steady_clock::now();
    uint32_t processed = 0;

    while (true)
    {
        std::unique_ptr<Request> request;
        {
            std::lock_guard<std::mutex> lock(m_readyMutex);
            if (m_ready.empty())
            {
                break;
            }
            request = std::move(m_ready.front());
            m_ready.pop_front();
        }

        finish(*request);
        m_pending.fetch_sub(1);
        ++processed;

        const float elapsedMs = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsedMs >= budgetMs)
        {
            break;
        }
    }

    const float totalMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    TracyPlot("Asset::PumpProcessed", static_cast<int64_t>(processed));
    TracyPlot("Asset::PumpMs", static_cast<double>(totalMs));
}

void AssetLoader::finish(Request& request)
{
    if (request.isMesh)
    {
        ZoneScopedN("Asset::FinishMesh");
        ZoneText(request.path.c_str(), request.path.size());

        if (!request.meshData)
        {
            LOG_ERROR("AssetLoader: failed to load mesh: " + request.path);
            request.meshResource->setState(ResourceState::Failed);
            return;
        }

        Mesh* mesh = request.meshResource->get();
        *mesh = std::move(*request.meshData);
        mesh->ready = true;
        request.meshResource->setState(ResourceState::Ready);
        return;
    }

    ZoneScopedN("Asset::FinishTexture");
    ZoneText(request.path.c_str(), request.path.size());

    if (!request.textureData)
    {
        LOG_ERROR("AssetLoader: failed to load texture: " + request.path);
        request.textureResource->setState(ResourceState::Failed);
        return;
    }

    auto uploaded = TextureLoader::getInstance().createGPU(*request.textureData, request.path);
    request.textureData.reset();

    if (!uploaded)
    {
        request.textureResource->setState(ResourceState::Failed);
        return;
    }

    Texture* texture = request.textureResource->get();
    *texture = std::move(*uploaded);
    texture->ready = true;
    request.textureResource->setState(ResourceState::Ready);
}
