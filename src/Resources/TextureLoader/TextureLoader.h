#pragma once

#include "Resources/TextureLoader/Texture.h"
#include <memory>
#include <string>
#include "IRenderAdapter.h"


class TextureLoader {
public:
    TextureLoader() = default;
    ~TextureLoader() = default;

    static TextureLoader& getInstance()
    {
        static TextureLoader instance;
        return instance;
    }

    std::unique_ptr<TextureData> loadFromFile(const std::string& path);
    std::unique_ptr<Texture> loadAndCreateGPU(const std::string& path);

    void registerLoader(IRenderAdapter* renderAdapter);

private:
    void flipVertically(unsigned char* pixels, int width, int height, int channels);

	IRenderAdapter* m_renderAdapter;
};

inline std::unique_ptr<Texture> textureLoad(const std::string& path)
{
    return TextureLoader::getInstance().loadAndCreateGPU(path);
}