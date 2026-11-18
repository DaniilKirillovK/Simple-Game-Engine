#ifndef GAMEENGINE_SHADERPROGRAM_H
#define GAMEENGINE_SHADERPROGRAM_H
#include <string>
#include <cstdint>

enum class ShaderType
{
    Vertex,
    Fragment,
    None
};

struct Shader
{
    uint32_t shaderId = 0;
    std::string shaderPath = "";
    ShaderType shaderType = ShaderType::None;
};

struct ShaderProgram
{
    uint32_t vertex = 0;
    uint32_t pixel = 0;
};

#endif //GAMEENGINE_SHADERPROGRAM_H
