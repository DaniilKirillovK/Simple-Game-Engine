#pragma once
#include "Resource.h"
#include "Logger.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <typeindex>
#include <any>
#include <algorithm>

// Cache type: map<path, shared_ptr<Resource<T>>>
template<typename T>
using ResourceCache = std::unordered_map<std::string, std::shared_ptr<Resource<T>>>;

// Loader list type
template<typename T>
using LoaderList = std::vector<std::pair<int, std::function<std::unique_ptr<T>(const std::string&)>>>;


class ResourceManager 
{
public:
    static ResourceManager& getInstance() 
    {
        static ResourceManager instance;
        return instance;
    }

    template<typename T>
    std::shared_ptr<Resource<T>> load(const std::string& path);

    template<typename T>
    void registerLoader(std::function<std::unique_ptr<T>(const std::string&)> loader, int priority = 0);

    template<typename T>
    std::shared_ptr<Resource<T>> get(const std::string& path);

    template<typename T>
    bool isLoaded(const std::string& path);

    template<typename T>
    void unload(const std::string& path);

    template<typename T>
    void unloadAll();

    void unloadAllResources();

    template<typename T>
    size_t getCacheSize() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto& cache = const_cast<ResourceManager*>(this)->getCache<T>();
        return cache.size();
    }

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    template<typename T>
    ResourceCache<T>& getCache();

    template<typename T>
    LoaderList<T>& getLoaders();

    mutable std::mutex mutex;
    std::unordered_map<std::type_index, std::any> caches;
    std::unordered_map<std::type_index, std::any> loaders;
};

template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::load(const std::string& path)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto& cache = getCache<T>();

        auto it = cache.find(path);
        if (it != cache.end())
        {
            auto resource = it->second;
            if (resource)
            {
                LOG_RESOURCEMANAGER(std::string("Cache hit: ") + path);
                return resource;
            }
            else
            {
                cache.erase(it);
            }
        }

        LOG_RESOURCEMANAGER(std::string("Loading: ") + path);

        std::unique_ptr<T> data;

        auto& loaders = getLoaders<T>();
        if (loaders.empty())
        {
            LOG_ERROR(std::string("No loader registered for type: ") + std::string(typeid(T).name()));
            return nullptr;
        }

        // Use the highest priority loader (first in sorted list)
        data = loaders.front().second(path);

        if (!data) 
        {
            LOG_ERROR(std::string("Failed to load: ") + path);
            return nullptr;
        }

        auto resource = std::make_shared<Resource<T>>(path, std::move(data));
        cache[path] = resource;

        return resource;
    }
}

template<typename T>
void ResourceManager::registerLoader(std::function<std::unique_ptr<T>(const std::string&)> loader, int priority)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto& loaders = getLoaders<T>();
    loaders.push_back({ priority, loader });

    std::sort(loaders.begin(), loaders.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    LOG_RESOURCEMANAGER(std::string("Loader registered for type: ") + std::string(typeid(T).name()));
}

template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::get(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto& cache = getCache<T>();
    auto it = cache.find(path);
    if (it != cache.end()) 
    {
        auto resource = it->second;
        if (resource) 
        {
            return resource;
        }
        else 
        {
            cache.erase(it);
        }
    }

    return nullptr;
}

template<typename T>
bool ResourceManager::isLoaded(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto& cache = getCache<T>();
    auto it = cache.find(path);
    return it != cache.end() && it->second != nullptr;
}

template<typename T>
void ResourceManager::unload(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto& cache = getCache<T>();
    auto it = cache.find(path);
    if (it != cache.end()) 
    {
        cache.erase(it);

        LOG_RESOURCEMANAGER(std::string("Unloaded: ") + std::string(path));
    }
}

template<typename T>
void ResourceManager::unloadAll()
{
    std::lock_guard<std::mutex> lock(mutex);

    auto& cache = getCache<T>();
    size_t count = cache.size();
    cache.clear();

    LOG_RESOURCEMANAGER(std::string("Unloaded ") + std::to_string(count) + std::string(" resources of type: ") + std::string(typeid(T).name()));
}

template<typename T>
ResourceCache<T>& ResourceManager::getCache()
{
    std::type_index typeIdx = std::type_index(typeid(T));
    auto it = caches.find(typeIdx);
    if (it == caches.end()) {
        caches[typeIdx] = ResourceCache<T>();
    }
    return std::any_cast<ResourceCache<T>&>(caches[typeIdx]);
}

template<typename T>
LoaderList<T>& ResourceManager::getLoaders()
{
    std::type_index typeIdx = std::type_index(typeid(T));
    auto it = loaders.find(typeIdx);
    if (it == loaders.end()) 
    {
        loaders[typeIdx] = LoaderList<T>();
    }
    return std::any_cast<LoaderList<T>&>(loaders[typeIdx]);
}

#define RESOURCE_MANAGER ResourceManager::getInstance()
