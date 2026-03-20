#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Material : public Component
{
public:
    Material(glm::vec4 diffuseColor)
        : diffuseColor(diffuseColor) {}

    glm::vec4 diffuseColor = glm::vec4(1.0f);
    glm::vec4 specularColor = glm::vec4(0.5f);
    glm::vec4 ambientColor = glm::vec4(0.2f);
    float shininess = 32.0f;

    // Textures
    unsigned int diffuseTexture = 0;
    unsigned int specularTexture = 0;
    bool hasTexture = false;
};