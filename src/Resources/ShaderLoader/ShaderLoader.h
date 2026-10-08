#ifndef GAMEENGINE_SHADERLOADER_H
#define GAMEENGINE_SHADERLOADER_H
#include <memory>
#include <string>
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "IRenderAdapter.h"


struct ShaderSource
{
    std::string path;
    std::string source;
    ShaderType type = ShaderType::None;
};

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

    bool readSource(const std::string& shaderPath, ShaderSource& out);

    std::unique_ptr<Shader> createGPU(const ShaderSource& source);

    std::unique_ptr<Shader> loadFromFile(const std::string& shaderPath);

    void registerLoader(IRenderAdapter* renderAdapter);

private:
    std::string readFile(const std::string& path);

    IRenderAdapter* m_renderAdapter = nullptr;
};


inline std::unique_ptr<Shader> shaderLoad(const std::string& path)
{
    return ShaderLoader::getInstance().loadFromFile(path);
}


#endif //GAMEENGINE_SHADERLOADER_H
