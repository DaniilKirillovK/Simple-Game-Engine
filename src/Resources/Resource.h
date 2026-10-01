#pragma once
#include <atomic>
#include <string>
#include <memory>

// Жизненный цикл ресурса (ДОБАВЛЕНО для асинхронной загрузки).
//   Loading — объект создан (заглушка), данные ещё декодируются в фоне.
//   Ready   — данные загружены и готовы к использованию.
//   Failed  — загрузка не удалась (файл не найден / ошибка декодирования); объект остаётся
//             заглушкой, чтобы существующие указатели на него не стали висячими.
// Синхронная загрузка всегда даёт сразу Ready; Loading/Failed появляются только в
// асинхронном пути (AssetLoader).
enum class ResourceState : int
{
    Loading,
    Ready,
    Failed
};

template<typename T>
class Resource {
public:
    // Конструктор без данных: ресурс-«пустышка», ожидающий загрузки, поэтому состояние Loading.
    explicit Resource(const std::string& path)
        : path(path), data(nullptr), refCount(0), state(ResourceState::Loading)
    {}

    // Конструктор с готовыми данными: ресурс сразу Ready.
    Resource(const std::string& path, std::unique_ptr<T> data)
        : path(path), data(std::move(data)), refCount(0), state(ResourceState::Ready)
    {}

    ~Resource() = default;

    T* get() const { return data.get(); }
    T* operator->() const { return data.get(); }
    T& operator*() const { return *data; }

    bool isValid() const { return data != nullptr; }

    const std::string& getPath() const { return path; }

    // Состояние атомарное: его выставляет главный поток (в pump), но читать его безопасно
    // из любого потока. acquire/release гарантируют, что при виде Ready все записи данных,
    // сделанные до setState(Ready), тоже видны читающему потоку.
    ResourceState getState() const { return state.load(std::memory_order_acquire); }
    bool isReady() const { return getState() == ResourceState::Ready; }
    void setState(ResourceState newState) { state.store(newState, std::memory_order_release); }

    void addRef() { ++refCount; }
    void release() { --refCount; }
    int getRefCount() const { return refCount; }

    // ИСПРАВЛЕНИЕ бага. Раньше параметр назывался так же, как поле (data), и строка
    // «data = std::move(data)» присваивала параметр самому себе, а поле класса не менялось
    // вообще. Теперь параметр переименован в newData. Дополнительно выставляется состояние:
    // Ready, если данные есть, и Failed, если передали nullptr.
    void setData(std::unique_ptr<T> newData)
    {
        data = std::move(newData);
        setState(data ? ResourceState::Ready : ResourceState::Failed);
    }

private:
    std::string path;
    std::unique_ptr<T> data;
    int refCount;
    std::atomic<ResourceState> state;
};
