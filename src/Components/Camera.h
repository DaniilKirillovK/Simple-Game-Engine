#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components/Transform.h"

class Camera : public Component
{
public:
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

    float fov = 45.0f;
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    bool isActive = true;

    glm::mat4 getViewMatrix(const Transform& transform) const 
    {
        glm::vec3 eye = transform.position;
        glm::vec3 center = eye + forward;
        return glm::lookAt(eye, center, up);
    }

    glm::mat4 getProjectionMatrix() const 
    {
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }
};