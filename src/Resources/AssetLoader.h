#pragma once

#include "Resources/Resource.h"
#include "JobSystem/JobSystem.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

class IRenderAdapter;
struct Mesh;
struct Texture;
struct TextureData;

class AssetLoader
{
public:
    static AssetLoader& getInstance()
    {
        static AssetLoader instance;
        return instance;
    }

    void initialize(IRenderAdapter* renderer);

    void shutdown();

    std::shared_ptr<Resource<Mesh>> requestMesh(const std::string& path);
    std::shared_ptr<Resource<Texture>> requestTexture(const std::string& path);

    void pump(float budgetMs);

    uint32_t pendingCount() const { return m_pending.load(); }

private:
    AssetLoader();
    ~AssetLoader();

    struct Request
    {
        bool isMesh = true;
        std::string path;

        std::shared_ptr<Resource<Mesh>> meshResource;
        std::shared_ptr<Resource<Texture>> textureResource;

        std::unique_ptr<Mesh> meshData;
        std::unique_ptr<TextureData> textureData;
    };

    static void decodeJob(void* raw, uint32_t index);

    void dispatch(std::unique_ptr<Request> request);
    void complete(Request* request);
    void finish(Request& request);

    IRenderAdapter* m_renderer = nullptr;
    uint32_t m_placeholderTexture = 0;

    std::atomic<bool> m_stopping{ false };
    std::atomic<uint32_t> m_pending{ 0 };
    std::atomic<uint32_t> m_inFlight{ 0 };

    std::mutex m_readyMutex;
    std::deque<std::unique_ptr<Request>> m_ready;
};
