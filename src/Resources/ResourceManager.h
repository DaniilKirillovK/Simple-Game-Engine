#pragma once
#include "Resource.h"
#include "Logger.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <typeindex>
#include <any>
#include <algorithm>
#include <mutex>
#include <vector>
#include <utility>
#include "tracy/Tracy.hpp"

// Cache type: map<path, shared_ptr<Resource<T>>>
template<typename T>
using ResourceCache = std::unordered_map<std::string, std::shared_ptr<Resource<T>>>;

template<typename T>
using LoaderFunc = std::function<std::unique_ptr<T>(const std::string&)>;

template<typename T>
using LoaderList = std::vector<std::pair<int, LoaderFunc<T>>>;


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
    std::shared_ptr<Resource<T>> insert(const std::string& path, std::unique_ptr<T> data);

    template<typename T>
    void registerLoader(LoaderFunc<T> loader, int priority = 0);

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
        std::lock_guard<std::mutex> lock(m_mutex);
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

    mutable std::mutex m_mutex;
    std::unordered_map<std::type_index, std::any> caches;
    std::unordered_map<std::type_index, std::any> loaders;
};

template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::load(const std::string& path)
{
    LoaderFunc<T> loader;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto& cache = getCache<T>();
        auto it = cache.find(path);
        if (it != cache.end())
        {
            if (it->second)
            {
                return it->second;
            }
            cache.erase(it);
        }

        auto& registered = getLoaders<T>();
        if (registered.empty())
        {
            LOG_ERROR(std::string("No loader registered for type: ") + std::string(typeid(T).name()));
            return nullptr;
        }

        loader = registered.front().second;
    }

    ZoneScopedN("ResourceManager::load (sync)");
    ZoneText(path.c_str(), path.size());

    LOG_RESOURCEMANAGER(std::string("Loading: ") + path);

    std::unique_ptr<T> data = loader(path);
    if (!data)
    {
        LOG_ERROR(std::string("Failed to load: ") + path);
        return nullptr;
    }

    return insert<T>(path, std::move(data));
}

template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::insert(const std::string& path, std::unique_ptr<T> data)
{
    if (!data)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto& cache = getCache<T>();
    auto it = cache.find(path);
    if (it != cache.end() && it->second)
    {
        return it->second;
    }

    auto resource = std::make_shared<Resource<T>>(path, std::move(data));
    cache[path] = resource;

    return resource;
}

template<typename T>
void ResourceManager::registerLoader(LoaderFunc<T> loader, int priority)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto& registered = getLoaders<T>();
        registered.push_back({ priority, std::move(loader) });

        std::sort(registered.begin(), registered.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
    }

    LOG_RESOURCEMANAGER(std::string("Loader registered for type: ") + std::string(typeid(T).name()));
}

template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::get(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& cache = getCache<T>();
    auto it = cache.find(path);
    if (it != cache.end())
    {
        if (it->second)
        {
            return it->second;
        }
        cache.erase(it);
    }

    return nullptr;
}

template<typename T>
bool ResourceManager::isLoaded(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& cache = getCache<T>();
    auto it = cache.find(path);
    return it != cache.end() && it->second != nullptr;
}

template<typename T>
void ResourceManager::unload(const std::string& path)
{
    bool erased = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto& cache = getCache<T>();
        auto it = cache.find(path);
        if (it != cache.end())
        {
            cache.erase(it);
            erased = true;
        }
    }

    if (erased)
    {
        LOG_RESOURCEMANAGER(std::string("Unloaded: ") + std::string(path));
    }
}

template<typename T>
void ResourceManager::unloadAll()
{
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto& cache = getCache<T>();
        count = cache.size();
        cache.clear();
    }

    LOG_RESOURCEMANAGER(std::string("Unloaded ") + std::to_string(count) + std::string(" resources of type: ") + std::string(typeid(T).name()));
}

template<typename T>
ResourceCache<T>& ResourceManager::getCache()
{
    std::type_index typeIdx = std::type_index(typeid(T));
    auto it = caches.find(typeIdx);
    if (it == caches.end()) 
    {
        it = caches.emplace(typeIdx, ResourceCache<T>()).first;
    }
    return std::any_cast<ResourceCache<T>&>(it->second);
}

template<typename T>
LoaderList<T>& ResourceManager::getLoaders()
{
    std::type_index typeIdx = std::type_index(typeid(T));
    auto it = loaders.find(typeIdx);
    if (it == loaders.end())
    {
        it = loaders.emplace(typeIdx, LoaderList<T>()).first;
    }
    return std::any_cast<LoaderList<T>&>(it->second);
}

#define RESOURCE_MANAGER ResourceManager::getInstance()
