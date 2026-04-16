#include "Resources/ResourceManager.h"
#include "Logger.h"
#include <string>
#include <algorithm>
#include <utility>

void ResourceManager::unloadAllResources()
{
    std::lock_guard<std::mutex> lock(mutex);

    caches.clear();

    LOG_RESOURCEMANAGER("All resources unloaded");
}
