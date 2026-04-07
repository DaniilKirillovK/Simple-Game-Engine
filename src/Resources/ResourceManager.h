#pragma once
#include "Resource.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <typeindex>
#include <any>

// Cache type: map<path, weak_ptr<Resource<T>>>
template<typename T>
using ResourceCache = std::unordered_map<std::string, std::weak_ptr<Resource<T>>>;

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
    size_t getCacheSize() const;

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

#define RESOURCE_MANAGER ResourceManager::getInstance()
