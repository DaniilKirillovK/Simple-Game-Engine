#define STB_IMAGE_IMPLEMENTATION

#include "Resources/TextureLoader/TextureLoader.h"
#include "IRenderAdapter.h"
#include <libs/stb_image/stb_image.h>
#include <iostream>
#include <algorithm>
#include "Logger.h"
#include "Resources/ResourceManager.h"
#include "OpenGLRenderAdapter.h"
#include "tracy/Tracy.hpp"

std::unique_ptr<TextureData> TextureLoader::loadFromFile(const std::string& path)
{
    // TRACY: стадия 1 (чтение + декодирование stb_image). Зона одинаково видна и при
    // синхронной загрузке (на строке главного потока — это и есть «фриз»), и в воркере
    // при асинхронной, поэтому по ней удобно сравнивать «до» и «после».
    ZoneScopedN("TextureLoader::loadFromFile");
    ZoneText(path.c_str(), path.size());

    // Здесь раньше вызывалась stbi_set_flip_vertically_on_load(false). Эта функция пишет в
    // ГЛОБАЛЬНУЮ переменную stb, а loadFromFile теперь выполняется в нескольких воркерах
    // одновременно — это формальная гонка данных. Значение всегда одно и то же (false),
    // поэтому настройку перенесли в registerLoader: она выполняется один раз в главном потоке.
    int width, height, channels;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);

    if (!pixels) 
    {
        LOG_RESOURCEMANAGER_ERROR("TextureLoader: Failed to load texture from path: " + path + " | Error: " + stbi_failure_reason());
        return nullptr;
    }

    LOG_RESOURCEMANAGER("TextureLoader: Loaded " + path + " | Size: " + std::to_string(width) + "x" + std::to_string(height) + " | Channels: " + std::to_string(channels));

    auto textureData = std::make_unique<TextureData>(width, height, channels, pixels);
    return textureData;
}

// СТАДИЯ 2 (GPU): выделена из бывшей loadAndCreateGPU без изменения логики.
// Создаёт GL-текстуру из уже декодированных пикселей. Обязана вызываться в потоке с
// GL-контекстом — в асинхронном пути это AssetLoader::pump (главный поток).
std::unique_ptr<Texture> TextureLoader::createGPU(const TextureData& data, const std::string& path)
{
    // TRACY: стадия 2 (загрузка в видеопамять). Всегда в главном потоке.
    ZoneScopedN("TextureLoader::createGPU");
    ZoneText(path.c_str(), path.size());

    // Защита: нет пикселей (декодирование не удалось) или рендерер не зарегистрирован.
    if (!data.pixels || !m_renderAdapter)
    {
        return nullptr;
    }

    uint32_t handle = m_renderAdapter->createTexture(data);
    if (handle == 0)
    {
        LOG_RESOURCEMANAGER_ERROR("TextureLoader: Failed to create GPU texture for: " + path);
        return nullptr;
    }

    return std::make_unique<Texture>(handle, data, path);
}

// Синхронный путь: обе стадии подряд в одном потоке. Поведение то же, что было раньше,
// теперь это просто композиция loadFromFile + createGPU.
std::unique_ptr<Texture> TextureLoader::loadAndCreateGPU(const std::string& path)
{
    auto textureData = loadFromFile(path);
    if (!textureData)
    {
        return nullptr;
    }

    return createGPU(*textureData, path);
}

void TextureLoader::registerLoader(IRenderAdapter* renderAdapter)
{
    m_renderAdapter = renderAdapter;

    // Глобальная настройка stb задаётся один раз здесь, в главном потоке при старте,
    // и никогда из воркеров (см. комментарий в loadFromFile).
    stbi_set_flip_vertically_on_load(false);

    RESOURCE_MANAGER.registerLoader<Texture>(&textureLoad, 0);
    renderAdapter->loadAssetIcons();

    LOG_INFO("TextureLoader registered with ResourceManager");
}

void TextureLoader::flipVertically(unsigned char* pixels, int width, int height, int channels) 
{
    int rowSize = width * channels;
    std::vector<unsigned char> temp(rowSize);

    for (int y = 0; y < height / 2; ++y) 
    {
        unsigned char* top = pixels + y * rowSize;
        unsigned char* bottom = pixels + (height - 1 - y) * rowSize;

        memcpy(temp.data(), top, rowSize);
        memcpy(top, bottom, rowSize);
        memcpy(bottom, temp.data(), rowSize);
    }
}
