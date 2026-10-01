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
#include "tracy/Tracy.hpp" // зона синхронной загрузки в load()

// Cache type: map<path, shared_ptr<Resource<T>>>
template<typename T>
using ResourceCache = std::unordered_map<std::string, std::shared_ptr<Resource<T>>>;

// Тип функции-загрузчика (вынесен в отдельный псевдоним, чтобы копировать загрузчик
// из списка и вызывать его уже вне мьютекса). Список загрузчиков: пары (приоритет, функция).
template<typename T>
using LoaderFunc = std::function<std::unique_ptr<T>(const std::string&)>;

template<typename T>
using LoaderList = std::vector<std::pair<int, LoaderFunc<T>>>;


// ПОТОКОБЕЗОПАСНОСТЬ (добавлено в рамках подготовки к параллельной загрузке).
// Раньше кэши лежали в обычных unordered_map без какой-либо защиты: одновременный доступ
// из двух потоков привёл бы к гонке. Теперь каждый публичный метод берёт m_mutex, поэтому
// менеджером можно пользоваться и из главного потока, и из воркеров job system.
// Принцип: загрузчик (loader) вызывается ВСЕГДА БЕЗ удержания мьютекса — загрузка может
// быть долгой, и пока один поток грузит файл, остальные не должны стоять на блокировке.
// Результат публикуется в кэш уже под мьютексом.
class ResourceManager
{
public:
    static ResourceManager& getInstance()
    {
        static ResourceManager instance;
        return instance;
    }

    // Возвращает ресурс из кэша либо загружает его СИНХРОННО через зарегистрированный загрузчик
    // (блокирует вызывающий поток). Асинхронный путь — через AssetLoader.
    template<typename T>
    std::shared_ptr<Resource<T>> load(const std::string& path);

    // НОВЫЙ метод. Кладёт уже готовые данные в кэш под ключом path (без вызова загрузчика).
    // Нужен AssetLoader: он создаёт заглушку и регистрирует её в менеджере. Если путь уже
    // есть в кэше, побеждает существующий ресурс, а переданные data отбрасываются
    // (так два запроса одного пути не создадут два разных объекта).
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

    // Внутренние функции доступа к картам. Сами НЕ блокируют: вызывающий обязан уже держать
    // m_mutex. (Блокировка внутри привела бы к дедлоку при повторном захвате того же мьютекса.)
    template<typename T>
    ResourceCache<T>& getCache();

    template<typename T>
    LoaderList<T>& getLoaders();

    // Один мьютекс на все кэши и список загрузчиков. mutable — чтобы можно было брать его
    // в const-методе getCacheSize. (Раньше код обращался к переменной «mutex», которой в
    // классе не существовало; это не ломало сборку лишь потому, что шаблоны не инстанцировались.)
    mutable std::mutex m_mutex;
    std::unordered_map<std::type_index, std::any> caches;
    std::unordered_map<std::type_index, std::any> loaders;
};

template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::load(const std::string& path)
{
    // ЭТАП 1 (под мьютексом): проверка кэша и выбор загрузчика.
    // Загрузчик копируем в локальную переменную, чтобы вызвать его уже без блокировки.
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

    // TRACY: зона СИНХРОННОЙ загрузки. Стоит после проверки кэша, поэтому появляется только
    // при реальной загрузке (промах кэша), а не на каждом обращении. Длинная зона на строке
    // главного потока = фриз кадра. Именно её нужно показать на трейсе «до» и убедиться,
    // что после перехода на AssetLoader она исчезает из горячих мест (сцена, окно ассетов).
    ZoneScopedN("ResourceManager::load (sync)");
    ZoneText(path.c_str(), path.size());

    LOG_RESOURCEMANAGER(std::string("Loading: ") + path);

    // ЭТАП 2 (БЕЗ мьютекса): сама загрузка. Она может занять десятки миллисекунд, и в это
    // время другие потоки должны иметь возможность работать с менеджером.
    std::unique_ptr<T> data = loader(path);
    if (!data)
    {
        LOG_ERROR(std::string("Failed to load: ") + path);
        return nullptr;
    }

    // ЭТАП 3 (под мьютексом внутри insert): публикация в кэш. Если за время загрузки другой
    // поток успел положить тот же путь, insert вернёт уже существующий ресурс.
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

    // Путь уже занят — возвращаем существующий ресурс, новые данные отбрасываются.
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
    // Мьютекс держим только на изменение списка загрузчиков; лог пишем уже после выхода из
    // блока (лог тоже под мьютексом, и держать два мьютекса сразу без нужды не стоит).
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
    // Флаг нужен, чтобы записать в лог уже ПОСЛЕ освобождения мьютекса (см. registerLoader).
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
    if (it == caches.end()) {
        // Раньше здесь был двойной поиск (caches[typeIdx] дважды); теперь один emplace, и
        // итератор переиспользуется для any_cast ниже.
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
