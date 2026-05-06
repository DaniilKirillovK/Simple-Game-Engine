#pragma once
#include <cstdint>
#include <string>

struct TextureData 
{
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = nullptr;

    TextureData() = default;

    TextureData(int w, int h, int c, unsigned char* p)
        : width(w), height(h), channels(c), pixels(p) {
    }

    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;

    TextureData(TextureData&& other) noexcept
        : width(other.width), height(other.height), channels(other.channels), pixels(other.pixels) 
    {
        other.pixels = nullptr;
    }

    TextureData& operator=(TextureData&& other) noexcept 
    {
        if (this != &other) 
        {
            cleanup();
            width = other.width;
            height = other.height;
            channels = other.channels;
            pixels = other.pixels;
            other.pixels = nullptr;
        }
        return *this;
    }

    ~TextureData() 
    {
        cleanup();
    }

    void cleanup() 
    {
        if (pixels) 
        {
            delete[] pixels;
            pixels = nullptr;
        }
    }

    uint32_t getInternalFormat() const;

    uint32_t getPixelFormat() const;
};

struct Texture 
{
public:
    uint32_t m_handle = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    std::string m_path;

    Texture() = default;
    Texture(uint32_t handle, const TextureData& data, const std::string& path)
        : m_handle(handle)
        , m_width(data.width)
        , m_height(data.height)
        , m_channels(data.channels)
        , m_path(path) {
    }

    ~Texture() = default;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept
    : m_handle(other.m_handle)
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_channels(other.m_channels)
        , m_path(std::move(other.m_path)) 
    {
        other.m_handle = 0;
    }

    Texture& operator=(Texture&& other) noexcept 
    {
        if (this != &other) {
            m_handle = other.m_handle;
            m_width = other.m_width;
            m_height = other.m_height;
            m_channels = other.m_channels;
            m_path = std::move(other.m_path);
            other.m_handle = 0;
        }
        return *this;
    }

    bool isValid() const { return m_handle != 0; }

    void bind(int slot = 0) const;
    void unbind() const;
};
