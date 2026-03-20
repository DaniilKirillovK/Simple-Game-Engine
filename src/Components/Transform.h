#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp> 

class Transform : public Component 
{
public:
    Transform() {};
    Transform(glm::vec3 position, glm::quat rotation, glm::vec3 scale, glm::vec3 eulerRotation)
        : position(position), rotation(rotation), scale(scale), eulerRotation(eulerRotation) {}

    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    glm::vec3 eulerRotation = glm::vec3(0.0f);

    glm::mat4 getLocalMatrix() const 
    {
        glm::mat4 matrix = glm::mat4(1.0f);
        matrix = glm::translate(matrix, position);
        matrix = matrix * glm::mat4x4(rotation);
        matrix = glm::scale(matrix, scale);
        return matrix;
    }

    void updateQuaternion() 
    {
        rotation = glm::quat(glm::radians(eulerRotation));
    }
};