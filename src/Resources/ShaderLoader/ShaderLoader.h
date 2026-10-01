#ifndef GAMEENGINE_SHADERLOADER_H
#define GAMEENGINE_SHADERLOADER_H
#include <memory>
#include <string>
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "IRenderAdapter.h"


// НОВАЯ структура: результат CPU-стадии — исходный текст шейдера, готовый к компиляции
// (путь, текст и определённый тип: вершинный/фрагментный).
struct ShaderSource
{
    std::string path;
    std::string source;
    ShaderType type = ShaderType::None;
};

// ИЗМЕНЕНИЕ: загрузка шейдера разделена на две стадии (подготовка к параллельности).
//   1. readSource — чтение файла и определение типа шейдера. Только CPU, потокобезопасно.
//   2. createGPU  — компиляция шейдера (вызовы OpenGL), только в потоке с GL-контекстом.
// ВАЖНО: пока вызывающий код по-прежнему синхронный (loadFromFile = обе стадии подряд).
// Шейдеры не переведены в асинхронный режим: их мало, они маленькие, а Material в
// конструкторе сразу линкует программу и требует готовые GL-идентификаторы шейдеров.
class ShaderLoader
{
public:
    ShaderLoader() = default;
    ~ShaderLoader() = default;

    static ShaderLoader& getInstance()
    {
        static ShaderLoader instance;
        return instance;
    }

    // Стадия 1 (CPU). Из любого потока. Возвращает false, если файл не найден или пуст.
    bool readSource(const std::string& shaderPath, ShaderSource& out);

    // Стадия 2 (GPU). Только поток с GL-контекстом.
    std::unique_ptr<Shader> createGPU(const ShaderSource& source);

    // Обе стадии подряд — прежнее поведение; используется синхронным путём через ResourceManager.
    std::unique_ptr<Shader> loadFromFile(const std::string& shaderPath);

    void registerLoader(IRenderAdapter* renderAdapter);

private:
    std::string readFile(const std::string& path);

    // Раньше не был инициализирован. Теперь nullptr — createGPU проверяет это значение.
    IRenderAdapter* m_renderAdapter = nullptr;
};


inline std::unique_ptr<Shader> shaderLoad(const std::string& path)
{
    return ShaderLoader::getInstance().loadFromFile(path);
}


#endif //GAMEENGINE_SHADERLOADER_H
