#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Resources/TextureLoader/Texture.h"
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "IRenderAdapter.h"
#include "Logger.h"

class Material
{
public:
    Material(glm::vec4 diffuseColor, Shader* vertexShader, Shader* fragmentShader)
        : diffuseColor(diffuseColor)
    {
        shaderProgram = { vertexShader->shaderId, fragmentShader->shaderId };
        vertexShaderPath = vertexShader->shaderPath;
        fragmentShaderPath = fragmentShader->shaderPath;
    }

    glm::vec4 diffuseColor = glm::vec4(1.0f);
    glm::vec4 specularColor = glm::vec4(0.5f);
    glm::vec4 ambientColor = glm::vec4(0.2f);
    float shininess = 32.0f;

    Texture* diffuseTexture = nullptr;
    Texture* specularTexture = nullptr;

    std::string vertexShaderPath = "";
    std::string fragmentShaderPath = "";

    ShaderProgram shaderProgram;

    // Textures
    bool hasTexture = false;
};