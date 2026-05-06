#include "Resources/ResourceManager.h"
#include "Logger.h"
#include <string>
#include <algorithm>
#include <utility>

void ResourceManager::unloadAllResources()
{
    caches.clear();
    LOG_RESOURCEMANAGER("All resources unloaded");
}
