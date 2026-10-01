#include "Resources/ResourceManager.h"
#include "Logger.h"
#include <string>
#include <algorithm>
#include <utility>

void ResourceManager::unloadAllResources()
{
    // Очистка всех кэшей под мьютексом (в другом потоке мог идти поиск/вставка).
    // Лог пишется после выхода из блока, чтобы не держать мьютекс во время вывода.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        caches.clear();
    }
    LOG_RESOURCEMANAGER("All resources unloaded");
}
