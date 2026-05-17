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
    Material(glm::vec4 diffuseColor, Shader* vertexShader, Shader* fragmentShader, IRenderAdapter& renderAdapter)
        : diffuseColor(diffuseColor)
    {
        shaderProgram = new ShaderProgram();
        shaderProgram->programId = renderAdapter.linkShaderProgram(vertexShader->shaderId, fragmentShader->shaderId);

        vertexShaderPath = vertexShader->shaderPath;
        fragmentShaderPath = fragmentShader->shaderPath;
        
        UniformLocations uniforms;
        uniforms.modelMatrix = glGetUniformLocation(shaderProgram->programId, "uModelMatrix");
        uniforms.viewMatrix = glGetUniformLocation(shaderProgram->programId, "uViewMatrix");
        uniforms.projectionMatrix = glGetUniformLocation(shaderProgram->programId, "uProjectionMatrix");
        uniforms.normalMatrix = glGetUniformLocation(shaderProgram->programId, "uNormalMatrix");

        uniforms.cameraPosition = glGetUniformLocation(shaderProgram->programId, "uCameraPosition");

        uniforms.materialDiffuseColor = glGetUniformLocation(shaderProgram->programId, "uMaterialDiffuseColor");
        uniforms.materialSpecularColor = glGetUniformLocation(shaderProgram->programId, "uMaterialSpecularColor");
        uniforms.materialAmbientColor = glGetUniformLocation(shaderProgram->programId, "uMaterialAmbientColor");
        uniforms.materialShininess = glGetUniformLocation(shaderProgram->programId, "uMaterialShininess");
        uniforms.materialHasTexture = glGetUniformLocation(shaderProgram->programId, "uMaterialHasTexture");
        uniforms.texture = glGetUniformLocation(shaderProgram->programId, "uTexture");
        uniforms.numLights = glGetUniformLocation(shaderProgram->programId, "uNumLights");

        for (int i = 0; i < 8; i++)
        {
            std::string prefix = "uLights[" + std::to_string(i) + "].";
            uniforms.lightTypes[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "type").c_str());
            uniforms.lightColors[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "color").c_str());
            uniforms.lightIntensities[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "intensity").c_str());
            uniforms.lightPositions[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "position").c_str());
            uniforms.lightDirections[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "direction").c_str());
            uniforms.lightConstants[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "constant").c_str());
            uniforms.lightLinears[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "linear").c_str());
            uniforms.lightQuadratics[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "quadratic").c_str());
            uniforms.lightCutOffs[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "cutOff").c_str());
            uniforms.lightOuterCutOffs[i] = glGetUniformLocation(shaderProgram->programId, (prefix + "outerCutOff").c_str());
        }

        shaderProgram->uniforms = uniforms;
    }

    glm::vec4 diffuseColor = glm::vec4(1.0f);
    glm::vec4 specularColor = glm::vec4(0.5f);
    glm::vec4 ambientColor = glm::vec4(0.2f);
    float shininess = 32.0f;

    Texture* diffuseTexture = nullptr;
    Texture* specularTexture = nullptr;

    std::string vertexShaderPath = "";
    std::string fragmentShaderPath = "";

    ShaderProgram* shaderProgram = nullptr;

    // Textures
    bool hasTexture = false;
};