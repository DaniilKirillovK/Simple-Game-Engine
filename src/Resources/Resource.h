#pragma once
#include <atomic>
#include <string>
#include <memory>

enum class ResourceState : int
{
    Loading,
    Ready,
    Failed
};

template<typename T>
class Resource 
{
public:
    explicit Resource(const std::string& path)
        : path(path), data(nullptr), refCount(0), state(ResourceState::Loading)
    {}

    Resource(const std::string& path, std::unique_ptr<T> data)
        : path(path), data(std::move(data)), refCount(0), state(ResourceState::Ready)
    {}

    ~Resource() = default;

    T* get() const { return data.get(); }
    T* operator->() const { return data.get(); }
    T& operator*() const { return *data; }

    bool isValid() const { return data != nullptr; }

    const std::string& getPath() const { return path; }

    ResourceState getState() const { return state.load(std::memory_order_acquire); }
    bool isReady() const { return getState() == ResourceState::Ready; }
    void setState(ResourceState newState) { state.store(newState, std::memory_order_release); }

    void addRef() { ++refCount; }
    void release() { --refCount; }
    int getRefCount() const { return refCount; }

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
