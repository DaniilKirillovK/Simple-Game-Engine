#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class LightType 
{
    Directional,
    Point,
    Spot
};

class Light : public Component
{
public:
    LightType type = LightType::Directional;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;

    // Point light
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    float range = 50.0f;

    // Directional light
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);

    // Spot light
    float cutOff = glm::cos(glm::radians(12.5f));
    float outerCutOff = glm::cos(glm::radians(15.0f));

    glm::vec4 position = glm::vec4(0.0f);

    bool enabled = true;
};