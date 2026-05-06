#ifndef GAMEENGINE_SHADERLOADER_H
#define GAMEENGINE_SHADERLOADER_H
#include <memory>
#include <string>
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "IRenderAdapter.h"


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

    std::unique_ptr<Shader> loadFromFile(const std::string& shaderPath);

    void registerLoader(IRenderAdapter* renderAdapter);

private:
    std::string readFile(const std::string& path);

    IRenderAdapter* m_renderAdapter;
};


inline std::unique_ptr<Shader> shaderLoad(const std::string& path)
{
    return ShaderLoader::getInstance().loadFromFile(path);
}


#endif //GAMEENGINE_SHADERLOADER_H