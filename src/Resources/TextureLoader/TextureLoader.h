#pragma once

#include "Resources/TextureLoader/Texture.h"
#include <memory>
#include <string>
#include "IRenderAdapter.h"


// ИЗМЕНЕНИЕ: загрузка текстуры разделена на две стадии, чтобы первую можно было выполнять
// в воркере job system, а вторую — в главном потоке.
//   1. loadFromFile — чтение файла и декодирование stb_image (PNG/JPG -> массив пикселей).
//      Только CPU, GL не трогает, поэтому безопасно вызывать из любого потока.
//   2. createGPU    — загрузка пикселей в видеопамять (glTexImage2D). Работает с OpenGL,
//      а значит ТОЛЬКО в потоке, которому принадлежит GL-контекст (главный).
// Раньше всё делала одна функция loadAndCreateGPU, которую нельзя было распараллелить.
class TextureLoader {
public:
    TextureLoader() = default;
    ~TextureLoader() = default;

    static TextureLoader& getInstance()
    {
        static TextureLoader instance;
        return instance;
    }

    // Стадия 1 (CPU). Можно вызывать из любого потока, в том числе из воркера.
    std::unique_ptr<TextureData> loadFromFile(const std::string& path);

    // Стадия 2 (GPU). НОВАЯ функция, выделена из loadAndCreateGPU. Только поток с GL-контекстом.
    std::unique_ptr<Texture> createGPU(const TextureData& data, const std::string& path);

    // Обе стадии подряд — прежнее поведение. Используется синхронным путём через ResourceManager
    // (например, для иконок редактора, которые нужны сразу).
    std::unique_ptr<Texture> loadAndCreateGPU(const std::string& path);

    void registerLoader(IRenderAdapter* renderAdapter);

private:
    void flipVertically(unsigned char* pixels, int width, int height, int channels);

	// Раньше указатель не был инициализирован (мусор до вызова registerLoader). Теперь nullptr,
	// и createGPU безопасно возвращает nullptr, если рендерер ещё не зарегистрирован.
	IRenderAdapter* m_renderAdapter = nullptr;
};

inline std::unique_ptr<Texture> textureLoad(const std::string& path)
{
    return TextureLoader::getInstance().loadAndCreateGPU(path);
}
