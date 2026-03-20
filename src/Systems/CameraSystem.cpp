#include "Systems/CameraSystem.h"
#include "Components/Camera.h"
#include "Components/Transform.h"
#include "Common/OpenGLKeyCodes.h"
#include "World.h"
#include "Logger.h"

void CameraSystem::update(World& world, float deltaTime)
{
    auto cameras = world.getEntitiesWithComponent<Camera>();
    EntityId activeCamera = INVALID_ENTITY;

    for (auto& [entity, camera] : cameras) 
    {
        if (camera->isActive) 
        {
            activeCamera = entity;
            break;
        }
    }

    if (activeCamera == INVALID_ENTITY) return;

    Transform* transform = world.getComponent<Transform>(activeCamera);
    Camera* camera = world.getComponent<Camera>(activeCamera);
    if (!transform || !camera) return;

    float moveSpeed = 10.0f;
    float lookSpeed = 300.0f;

    if (inputHandler->isKeyPressed(OpenGLKeyCode::W)) 
    {
        transform->position += camera->forward * moveSpeed * deltaTime;
    }
    if (inputHandler->isKeyPressed(OpenGLKeyCode::S)) 
    {
        transform->position -= camera->forward * moveSpeed * deltaTime;
    }
    if (inputHandler->isKeyPressed(OpenGLKeyCode::A)) 
    {
        transform->position -= camera->right * moveSpeed * deltaTime;
    }
    if (inputHandler->isKeyPressed(OpenGLKeyCode::D)) 
    {
        transform->position += camera->right * moveSpeed * deltaTime;
    }
    if (inputHandler->isKeyPressed(OpenGLKeyCode::Space)) 
    {
        transform->position += glm::vec3(0.0f, moveSpeed * deltaTime, 0.0f);
    }
    if (inputHandler->isKeyPressed(OpenGLKeyCode::LeftCtrl))
    {
        transform->position -= glm::vec3(0.0f, moveSpeed * deltaTime, 0.0f);
    }

    static float yaw = -90.0f;
    static float pitch = 0.0f;
    static double lastX = 400, lastY = 300;
    static bool firstMouse = true;

    double xpos, ypos;
    xpos = inputHandler->getMouseX();
    ypos = inputHandler->getMouseY();

    if (firstMouse) 
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    if (inputHandler->isMouseButtonPressed(OpenGLMouseButton::Right)) 
    {
        yaw += xoffset * lookSpeed * deltaTime;
        pitch += yoffset * lookSpeed * deltaTime;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        camera->forward = glm::normalize(front);

        camera->right = glm::normalize(glm::cross(camera->forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        camera->up = glm::normalize(glm::cross(camera->right, camera->forward));
    }
}
