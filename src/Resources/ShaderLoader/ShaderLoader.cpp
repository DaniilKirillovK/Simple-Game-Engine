#include "ShaderLoader.h"
#include "ShaderProgram.h"
#include "IRenderAdapter.h"
#include "Logger.h"

#include <fstream>
#include <sstream>
#include "ShaderLoader.h"
#include "Resources/ResourceManager.h"
#include "OpenGLRenderAdapter.h"
#include "tracy/Tracy.hpp"

bool ShaderLoader::readSource(const std::string& shaderPath, ShaderSource& out)
{
    ZoneScopedN("ShaderLoader::readSource");
    ZoneText(shaderPath.c_str(), shaderPath.size());

    LOG_INFO("ShaderLoader: loading shader: " + shaderPath);

    out.source = readFile(shaderPath);
    if (out.source.empty())
    {
        LOG_ERROR("ShaderLoader: shader is empty: " + shaderPath);
        return false;
    }

    out.path = shaderPath;
    out.type = ShaderType::None;

    if (shaderPath.length() >= 4)
    {
        const std::string shaderTypeStr = shaderPath.substr(shaderPath.length() - 4);
        if (shaderTypeStr == "vert")
        {
            out.type = ShaderType::Vertex;
        }
        else if (shaderTypeStr == "frag")
        {
            out.type = ShaderType::Fragment;
        }
    }

    return true;
}

std::unique_ptr<Shader> ShaderLoader::createGPU(const ShaderSource& source)
{
    ZoneScopedN("ShaderLoader::createGPU");
    ZoneText(source.path.c_str(), source.path.size());

    if (!m_renderAdapter)
    {
        return nullptr;
    }

    const unsigned int shaderId = m_renderAdapter->compileShaderSource(source.source, source.type);

    if (shaderId == 0)
    {
        LOG_ERROR("ShaderLoader: shader compilation failed: " + source.path);
        return nullptr;
    }

    auto shader = std::make_unique<Shader>();
    shader->shaderId = shaderId;
    shader->shaderType = source.type;
    shader->shaderPath = source.path;

    LOG_RESOURCEMANAGER("Shader loaded and cached: " + source.path);
    return shader;
}

std::unique_ptr<Shader> ShaderLoader::loadFromFile(const std::string& shaderPath)
{
    ShaderSource source;
    if (!readSource(shaderPath, source))
    {
        return nullptr;
    }

    return createGPU(source);
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