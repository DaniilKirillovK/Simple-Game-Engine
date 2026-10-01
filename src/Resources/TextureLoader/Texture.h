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

    // ИСПРАВЛЕНИЕ освобождения памяти. Раньше тело было здесь же и делало delete[] pixels.
    // Но пиксели выделяет stbi_load (через malloc), а не new[]. Освобождать память другим
    // семейством функций — неопределённое поведение (в Release могло проявиться крашем).
    // Теперь реализация вынесена в Texture.cpp и вызывает stbi_image_free. Объявление здесь
    // осталось, чтобы не тянуть stb_image.h во все файлы, включающие Texture.h.
    void cleanup();

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

    // Признак «настоящая картинка уже загружена».
    // false — пока идёт фоновая загрузка: m_handle указывает на общую белую текстуру 1x1
    // (заглушка), а размеры равны 1x1. AssetLoader заполняет ЭТОТ ЖЕ объект на месте
    // (меняет m_handle, размеры и ставит ready = true), поэтому все Material, уже хранящие
    // указатель на текстуру, получают результат автоматически, без пересоздания.
    // По умолчанию true: текстуры, созданные синхронно, всегда готовы.
    bool ready = true;

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
        , ready(other.ready) // флаг обязан переезжать вместе с данными: AssetLoader заполняет заглушку через move-присваивание
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
            ready = other.ready; // см. комментарий в move-конструкторе
            other.m_handle = 0;
        }
        return *this;
    }

    bool isValid() const { return m_handle != 0; }

    void bind(int slot = 0) const;
    void unbind() const;
};
