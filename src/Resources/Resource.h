#pragma once
#include <string>
#include <memory>

template<typename T>
class Resource {
public:
    explicit Resource(const std::string& path)
        : path(path), data(nullptr), refCount(0)
    {}

    Resource(const std::string& path, std::unique_ptr<T> data)
        : path(path), data(std::move(data)), refCount(0)
    {}

    ~Resource() = default;

    T* get() const { return data.get(); }
    T* operator->() const { return data.get(); }
    T& operator*() const { return *data; }

    bool isValid() const { return data != nullptr; }

    const std::string& getPath() const { return path; }

    void addRef() { ++refCount; }
    void release() { --refCount; }
    int getRefCount() const { return refCount; }

    void setData(std::unique_ptr<T> data) 
    {
        data = std::move(data);
    }

private:
    std::string path;
    std::unique_ptr<T> data;
    int refCount;
};