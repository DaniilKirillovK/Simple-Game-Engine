//
// Created by semen on 14.04.2026.
//

#include "ShaderLoader.h"
#include "ShaderProgram.h"
#include "IRenderAdapter.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

std::string ShaderLoader::ReadFile(const std::string& path)
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

std::shared_ptr<ShaderProgram> ShaderLoader::Load(
    const std::string& vertexPath,
    const std::string& fragmentPath,
    IRenderAdapter* renderAdapter)
{
    if (renderAdapter == nullptr)
    {
        LOG_ERROR("ShaderLoader: renderAdapter is null");
        return nullptr;
    }

    LOG_INFO("ShaderLoader: loading shaders:");
    LOG_INFO("Vertex: " + vertexPath);
    LOG_INFO("Fragment: " + fragmentPath);

    const std::string vertexSource = ReadFile(vertexPath);
    const std::string fragmentSource = ReadFile(fragmentPath);

    if (vertexSource.empty())
    {
        LOG_ERROR("ShaderLoader: vertex shader is empty: " + vertexPath);
        return nullptr;
    }

    if (fragmentSource.empty())
    {
        LOG_ERROR("ShaderLoader: fragment shader is empty: " + fragmentPath);
        return nullptr;
    }

    const unsigned int vertexShader = renderAdapter->compileShaderSource(vertexSource, ShaderType::Vertex);

    if (vertexShader == 0)
    {
        LOG_ERROR("ShaderLoader: vertex shader compilation failed: " + vertexPath);
        return nullptr;
    }

    LOG_INFO("ShaderLoader: vertex shader compiled successfully");

    const unsigned int fragmentShader = renderAdapter->compileShaderSource(fragmentSource, ShaderType::Fragment);

    if (fragmentShader == 0)
    {
        LOG_ERROR("ShaderLoader: fragment shader compilation failed: " + fragmentPath);
        renderAdapter->deleteShaderObject(vertexShader);
        return nullptr;
    }

    LOG_INFO("ShaderLoader: fragment shader compiled successfully");

    const unsigned int program = renderAdapter->linkShaderProgram(vertexShader, fragmentShader);

    renderAdapter->deleteShaderObject(vertexShader);
    renderAdapter->deleteShaderObject(fragmentShader);

    if (program == 0)
    {
        LOG_ERROR("ShaderLoader: shader program link failed");
        return nullptr;
    }

    LOG_INFO("ShaderLoader: shader program linked successfully");

    auto shaderProgram = std::make_shared<ShaderProgram>();
    shaderProgram->programId = program;
    shaderProgram->vertexPath = vertexPath;
    shaderProgram->fragmentPath = fragmentPath;

    LOG_RESOURCEMANAGER("Shader loaded and cached: " + vertexPath + " | " + fragmentPath);

    return shaderProgram;
}