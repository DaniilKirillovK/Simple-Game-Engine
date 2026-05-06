#ifndef GAMEENGINE_SHADERPROGRAM_H
#define GAMEENGINE_SHADERPROGRAM_H
#include <string>
#include <glad/glad.h>

enum class ShaderType
{
    Vertex,
    Fragment,
    None
};

struct UniformLocations
{
    GLint modelMatrix = -1;
    GLint viewMatrix = -1;
    GLint projectionMatrix = -1;
    GLint normalMatrix = -1;

    GLint cameraPosition = -1;

    // Material
    GLint materialDiffuseColor = -1;
    GLint materialSpecularColor = -1;
    GLint materialAmbientColor = -1;
    GLint materialShininess = -1;
    GLint materialHasTexture = -1;
    GLint texture = -1;

    GLint numLights = -1;
    GLint lightTypes[8] = { -1 };
    GLint lightColors[8] = { -1 };
    GLint lightIntensities[8] = { -1 };
    GLint lightPositions[8] = { -1 };
    GLint lightDirections[8] = { -1 };
    GLint lightConstants[8] = { -1 };
    GLint lightLinears[8] = { -1 };
    GLint lightQuadratics[8] = { -1 };
    GLint lightCutOffs[8] = { -1 };
    GLint lightOuterCutOffs[8] = { -1 };
};

struct Shader
{
    uint32_t shaderId = 0;
    ShaderType shaderType = ShaderType::None;
};

struct ShaderProgram
{
    uint32_t programId = 0;
    UniformLocations uniforms;
};

#endif //GAMEENGINE_SHADERPROGRAM_H
