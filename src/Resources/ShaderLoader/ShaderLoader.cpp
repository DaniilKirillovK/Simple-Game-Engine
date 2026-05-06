#include "ShaderLoader.h"
#include "ShaderProgram.h"
#include "IRenderAdapter.h"
#include "Logger.h"

#include <fstream>
#include <sstream>
#include "ShaderLoader.h"
#include "Resources/ResourceManager.h"
#include "OpenGLRenderAdapter.h"

std::unique_ptr<Shader> ShaderLoader::loadFromFile(const std::string& shaderPath)
{
    LOG_INFO("ShaderLoader: loading shader: " + shaderPath);
    const std::string shaderSource = readFile(shaderPath);

    if (shaderSource.empty())
    {
        LOG_ERROR("ShaderLoader: shader is empty: " + shaderSource);
        return nullptr;
    }

    std::string shaderTypeStr = shaderPath.substr(shaderPath.length() - 4);
    ShaderType shaderType;
    if (shaderTypeStr == "vert")
    {
        shaderType = ShaderType::Vertex;
    }
    else if (shaderTypeStr == "frag")
    {
        shaderType = ShaderType::Fragment;
    }
    else
    {
        shaderType = ShaderType::None;
    }

    const unsigned int shaderId = m_renderAdapter->compileShaderSource(shaderSource, shaderType);

    if (shaderId == 0)
    {
        LOG_ERROR("ShaderLoader: shader compilation failed: " + shaderPath);
        return nullptr;
    }

    auto shader = std::make_unique<Shader>();
    shader->shaderId = shaderId;
    shader->shaderType = shaderType;

    LOG_RESOURCEMANAGER("Shader loaded and cached: " + shaderPath);
    return shader;
}

void ShaderLoader::registerLoader(IRenderAdapter* renderAdapter)
{
    m_renderAdapter = renderAdapter;

    RESOURCE_MANAGER.registerLoader<Shader>(&shaderLoad, 0);
    LOG_INFO("MeshLoader registered with ResourceManager");
}

std::string ShaderLoader::readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in);

    if (!file.is_open())
    {
        LOG_ERROR("ShaderLoader: failed to open file: " + path);
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    LOG_INFO("ShaderLoader: successfully read file: " + path);

    return buffer.str();
}