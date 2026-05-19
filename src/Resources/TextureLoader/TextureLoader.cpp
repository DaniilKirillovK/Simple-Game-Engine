#define STB_IMAGE_IMPLEMENTATION

#include "Resources/TextureLoader/TextureLoader.h"
#include "IRenderAdapter.h"
#include <libs/stb_image/stb_image.h>
#include <iostream>
#include <algorithm>
#include "Logger.h"
#include "Resources/ResourceManager.h"
#include "OpenGLRenderAdapter.h"

std::unique_ptr<TextureData> TextureLoader::loadFromFile(const std::string& path) 
{
    stbi_set_flip_vertically_on_load(false);

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

std::unique_ptr<Texture> TextureLoader::loadAndCreateGPU(const std::string& path)
{
    auto textureData = loadFromFile(path);
    if (!textureData || !textureData->pixels) 
    {
        return std::unique_ptr<Texture>();
    }

    uint32_t handle = m_renderAdapter->createTexture(*textureData);
    if (handle == 0) 
    {
        LOG_RESOURCEMANAGER_ERROR("TextureLoader: Failed to create GPU texture for: " + path);
        return std::unique_ptr<Texture>();
    }

    Texture texture(handle, *textureData, path);

    return std::make_unique<Texture>(std::move(texture));
}

void TextureLoader::registerLoader(IRenderAdapter* renderAdapter)
{
    m_renderAdapter = renderAdapter;

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
