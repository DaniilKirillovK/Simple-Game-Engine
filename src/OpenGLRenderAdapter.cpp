#include "OpenGLRenderAdapter.h"
#include "Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components/MeshRenderer.h"
#include "Components/Material.h"
#include "Components/Light.h"
#include "Components/Transform.h"
#include <glm/gtc/type_ptr.hpp> 

static OpenGLRenderAdapter* g_instance = nullptr;

// Vertex Shader
const std::string vertexShaderSource = R"(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
uniform mat4 uNormalMatrix;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = uModelMatrix * vec4(aPos, 1.0);
    vWorldPosition = worldPos.xyz;
    gl_Position = uProjectionMatrix * uViewMatrix * worldPos;
    
    vNormal = normalize(mat3(uNormalMatrix) * aNormal);
    vTexCoord = aTexCoord;
}
)";

// Pixel Shader
const std::string fragmentShaderSource = R"(
#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vTexCoord;

out vec4 FragColor;

uniform vec4 uMaterialDiffuseColor;
uniform vec4 uMaterialSpecularColor;
uniform vec4 uMaterialAmbientColor;
uniform float uMaterialShininess;
uniform int uMaterialHasTexture;

// Light
struct Light {
    int type;      // 0-directional, 1-point, 2-spot
    vec3 color;
    float intensity;
    vec3 position;
    vec3 direction;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
};

uniform int uNumLights;
uniform Light uLights[8];

// Camera
uniform vec3 uCameraPosition;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);
    
    vec3 result = vec3(0.0);

    vec3 globalAmbient = vec3(0.2);
    result += globalAmbient * uMaterialDiffuseColor.rgb;
    
    for (int i = 0; i < uNumLights; i++) 
    {
        Light light = uLights[i];
        
        vec3 lightDir;
        float attenuation = 1.0;
        
        if (light.type == 0) { // Directional light
            lightDir = normalize(-light.direction);
        } 
        else if (light.type == 1) { // Point light
            vec3 lightVec = light.position - vWorldPosition;
            float distance = length(lightVec);
            lightDir = normalize(lightVec);
            attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        } 
        else { // Spot light
            vec3 lightVec = light.position - vWorldPosition;
            float distance = length(lightVec);
            lightDir = normalize(lightVec);
            
            float theta = dot(lightDir, normalize(-light.direction));
            float epsilon = light.cutOff - light.outerCutOff;
            float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
            
            attenuation = intensity / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        }
        
        // Ambient
        vec3 ambient = light.color * light.intensity * uMaterialAmbientColor.rgb;
        
        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = light.color * light.intensity * diff * uMaterialDiffuseColor.rgb;
        
        // Specular
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), uMaterialShininess);
        vec3 specular = light.color * light.intensity * spec * uMaterialSpecularColor.rgb;
        
        result += (ambient + diffuse + specular) * attenuation;
    }
    
    FragColor = vec4(result, uMaterialDiffuseColor.a);
}
)";

OpenGLRenderAdapter::OpenGLRenderAdapter()
    : m_window(nullptr)
    , m_width(DEFAULT_WIDTH)
    , m_height(DEFAULT_HEIGHT)
{
    g_instance = this;
}

OpenGLRenderAdapter::~OpenGLRenderAdapter()
{
    shutdown();
}

bool OpenGLRenderAdapter::initialize(int width, int height)
{
    m_width = width;
    m_height = height;

    if (!initGLFW(width, height)) 
    {
        LOG_ERROR("GLFW initialization error");
        return false;
    }

    if (!initGLAD()) 
    {
        LOG_ERROR("GLAD initialization error");
        return false;
    }

    setupOpenGLState();

    compileShaders();

    std::string openGLVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::string openGLRenderer = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    LOG_INFO("OpenGL version: " + openGLVersion);
    LOG_INFO("OpenGL renderer: " + openGLRenderer);

    return true;
}

void OpenGLRenderAdapter::render()
{
    glUseProgram(m_shaderProgram);

    glm::mat4 viewMatrix = currentViewMatrix;
    glm::vec3 cameraPosition = glm::vec3(
        -viewMatrix[3][0] * viewMatrix[0][0] - viewMatrix[3][1] * viewMatrix[1][0] - viewMatrix[3][2] * viewMatrix[2][0],
        -viewMatrix[3][0] * viewMatrix[0][1] - viewMatrix[3][1] * viewMatrix[1][1] - viewMatrix[3][2] * viewMatrix[2][1],
        -viewMatrix[3][0] * viewMatrix[0][2] - viewMatrix[3][1] * viewMatrix[1][2] - viewMatrix[3][2] * viewMatrix[2][2]
    );

    GLint cameraPosLoc = glGetUniformLocation(m_shaderProgram, "uCameraPosition");
    if (cameraPosLoc != -1) 
    {
        glUniform3f(cameraPosLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);
    }

    glfwSwapBuffers(m_window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderAdapter::shutdown()
{
    LOG_INFO("OpenGL shutting down");

    // Clear resources
    for (auto& [mesh, vao] : meshVAOs) 
    {
        glDeleteVertexArrays(1, &vao);
    }
    for (auto& [mesh, vbo] : meshVBOs) 
    {
        glDeleteBuffers(1, &vbo);
    }
    for (auto& [mesh, ebo] : meshEBOs) 
    {
        glDeleteBuffers(1, &ebo);
    }
    meshVAOs.clear();
    meshVBOs.clear();
    meshEBOs.clear();

    if (m_shaderProgram) 
    {
        glDeleteProgram(m_shaderProgram);
    }

    if (m_window) 
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void OpenGLRenderAdapter::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    if (g_instance)
    {
        LOG_INFO("Frame buffer resize");
        g_instance->onFramebufferResize(width, height);
    }
}

void OpenGLRenderAdapter::onKey(int key, int action)
{
    KeyEvent event;
    event.keyCode = key;

    if (action == GLFW_PRESS) 
    {
        event.pressed = true;
    }
    else if (action == GLFW_RELEASE) 
    {
        event.pressed = false;
    }
    else 
    {
        return;
    }

    keyEvents.push_back(event);
}

void OpenGLRenderAdapter::onMouseButton(int button, int action)
{
    MouseButtonEvent event;
    event.button = button;

    if (action == GLFW_PRESS) 
    {
        event.pressed = true;
    }
    else 
    {
        event.pressed = false;
    }

    mouseButtonEvents.push_back(event);
}

void OpenGLRenderAdapter::onMouseMove(double x, double y)
{
    MouseMoveEvent event;
    event.x = x;
    event.y = y;
    mouseMoveEvents.push_back(event);
}

void OpenGLRenderAdapter::onMouseScroll(double xoffset, double yoffset)
{
    MouseScrollEvent event;
    event.xOffset = xoffset;
    event.yOffset = yoffset;
    mouseScrollEvents.push_back(event);
}

void OpenGLRenderAdapter::setModelMatrix(const float* matrix)
{
    currentModelMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(uniforms.modelMatrix, 1, GL_FALSE, matrix);
}

void OpenGLRenderAdapter::setViewMatrix(const float* matrix)
{
    currentViewMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(uniforms.viewMatrix, 1, GL_FALSE, matrix);
}

void OpenGLRenderAdapter::setProjectionMatrix(const float* matrix)
{
    currentProjectionMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(uniforms.projectionMatrix, 1, GL_FALSE, matrix);
}

void OpenGLRenderAdapter::setNormalMatrix(const float* matrix)
{
    currentNormalMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(uniforms.normalMatrix, 1, GL_FALSE, matrix);
}

float OpenGLRenderAdapter::getAspectRatio()
{
    return (float)m_width / m_height;
}

void OpenGLRenderAdapter::setMaterial(const Material* material)
{
    currentMaterial = material;

    if (uniforms.materialDiffuseColor != -1) 
    {
        glUniform4f(uniforms.materialDiffuseColor,
            material->diffuseColor.r, material->diffuseColor.g,
            material->diffuseColor.b, material->diffuseColor.a);
    }

    if (uniforms.materialSpecularColor != -1) 
    {
        glUniform4f(uniforms.materialSpecularColor,
            material->specularColor.r, material->specularColor.g,
            material->specularColor.b, material->specularColor.a);
    }

    if (uniforms.materialAmbientColor != -1) 
    {
        glUniform4f(uniforms.materialAmbientColor,
            material->ambientColor.r, material->ambientColor.g,
            material->ambientColor.b, material->ambientColor.a);
    }

    if (uniforms.materialShininess != -1) 
    {
        glUniform1f(uniforms.materialShininess, material->shininess);
    }

    if (uniforms.materialHasTexture != -1) {
        glUniform1i(uniforms.materialHasTexture, material->hasTexture ? 1 : 0);
    }
}

void OpenGLRenderAdapter::setLights(const std::vector<Light*>& lights)
{
    currentLights = lights;

    int numLights = std::min((int)lights.size(), 8);
    glUniform1i(uniforms.numLights, numLights);

    for (int i = 0; i < numLights; i++) 
    {
        const Light* light = lights[i];

        if (uniforms.lightTypes[i] != -1) 
        {
            glUniform1i(uniforms.lightTypes[i], static_cast<int>(light->type));
        }

        if (uniforms.lightColors[i] != -1) 
        {
            glUniform3f(uniforms.lightColors[i], light->color.r, light->color.g, light->color.b);
        }

        if (uniforms.lightIntensities[i] != -1) 
        {
            glUniform1f(uniforms.lightIntensities[i], light->intensity);
        }

        if (uniforms.lightPositions[i] != -1) 
        {
            glUniform3f(uniforms.lightPositions[i], light->position.x, light->position.y, light->position.z);
        }

        if (uniforms.lightDirections[i] != -1) 
        {
            glUniform3f(uniforms.lightDirections[i], light->direction.x, light->direction.y, light->direction.z);
        }

        if (uniforms.lightConstants[i] != -1) 
        {
            glUniform1f(uniforms.lightConstants[i], light->constant);
        }

        if (uniforms.lightLinears[i] != -1) 
        {
            glUniform1f(uniforms.lightLinears[i], light->linear);
        }

        if (uniforms.lightQuadratics[i] != -1) 
        {
            glUniform1f(uniforms.lightQuadratics[i], light->quadratic);
        }

        if (uniforms.lightCutOffs[i] != -1) 
        {
            glUniform1f(uniforms.lightCutOffs[i], light->cutOff);
        }

        if (uniforms.lightOuterCutOffs[i] != -1) 
        {
            glUniform1f(uniforms.lightOuterCutOffs[i], light->outerCutOff);
        }
    }
}

void OpenGLRenderAdapter::drawMesh(const Mesh* mesh)
{
    if (!mesh) return;

    createMeshVAO(const_cast<Mesh*>(mesh));

    GLuint VAO = meshVAOs[const_cast<Mesh*>(mesh)];
    glBindVertexArray(VAO);

    if (!mesh->indices.empty()) 
    {
        glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
    }
    else 
    {
        glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
    }

    glBindVertexArray(0);
}


bool OpenGLRenderAdapter::initGLFW(int width, int height)
{
    if (!glfwInit()) 
    {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    LOG_INFO("Requesting OpenGL debug context");

    m_window = glfwCreateWindow(width, height, "OpenGL Window", nullptr, nullptr);
    if (!m_window) 
    {
        return false;
    }

    glfwMakeContextCurrent(m_window);

    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetScrollCallback(m_window, scrollCallback);

    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

    return true;
}

bool OpenGLRenderAdapter::initGLAD()
{
    return gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0;
}

void OpenGLRenderAdapter::setupOpenGLState()
{
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
}

void OpenGLRenderAdapter::onFramebufferResize(int width, int height)
{
    m_width = width;
    m_height = height;

    glViewport(0, 0, width, height);
}

void OpenGLRenderAdapter::createMeshVAO(Mesh* mesh)
{
    if (!mesh) return;

    if (meshVAOs.find(mesh) != meshVAOs.end()) 
    {
        return;
    }

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    std::vector<float> interleavedData;
    for (size_t i = 0; i < mesh->vertices.size(); i++) 
    {
        interleavedData.push_back(mesh->vertices[i].x);
        interleavedData.push_back(mesh->vertices[i].y);
        interleavedData.push_back(mesh->vertices[i].z);

        if (!mesh->normals.empty()) 
        {
            interleavedData.push_back(mesh->normals[i].x);
            interleavedData.push_back(mesh->normals[i].y);
            interleavedData.push_back(mesh->normals[i].z);
        }
        else 
        {
            interleavedData.push_back(0.0f);
            interleavedData.push_back(1.0f);
            interleavedData.push_back(0.0f);
        }

        if (!mesh->texCoords.empty()) 
        {
            interleavedData.push_back(mesh->texCoords[i].x);
            interleavedData.push_back(mesh->texCoords[i].y);
        }
        else 
        {
            interleavedData.push_back(0.0f);
            interleavedData.push_back(0.0f);
        }
    }

    glBufferData(GL_ARRAY_BUFFER, interleavedData.size() * sizeof(float),
        interleavedData.data(), GL_STATIC_DRAW);

    if (!mesh->indices.empty()) 
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indices.size() * sizeof(unsigned int),
            mesh->indices.data(), GL_STATIC_DRAW);
    }

    size_t stride = 8 * sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    meshVAOs[mesh] = VAO;
    meshVBOs[mesh] = VBO;
    meshEBOs[mesh] = EBO;
}

bool OpenGLRenderAdapter::shouldClose() const
{
    return glfwWindowShouldClose(m_window);
}

void OpenGLRenderAdapter::compileShaders()
{
    const char* vertexSrc = vertexShaderSource.c_str();
    const char* fragmentSrc = fragmentShaderSource.c_str();

    // Vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, NULL);
    glCompileShader(vertexShader);
    if (!checkShaderCompileErrors(vertexShader, "VERTEX")) return;

    // Fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSrc, NULL);
    glCompileShader(fragmentShader);
    if (!checkShaderCompileErrors(fragmentShader, "FRAGMENT")) return;

    // Создание шейдерной программы
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    // Link check
    int success;
    char infoLog[512];
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) 
    {
        glGetProgramInfoLog(m_shaderProgram, 512, NULL, infoLog);
        LOG_ERROR("Shader program link error " + std::string(infoLog));
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(m_shaderProgram);

    uniforms.modelMatrix = glGetUniformLocation(m_shaderProgram, "uModelMatrix");
    uniforms.viewMatrix = glGetUniformLocation(m_shaderProgram, "uViewMatrix");
    uniforms.projectionMatrix = glGetUniformLocation(m_shaderProgram, "uProjectionMatrix");
    uniforms.normalMatrix = glGetUniformLocation(m_shaderProgram, "uNormalMatrix");

    uniforms.materialDiffuseColor = glGetUniformLocation(m_shaderProgram, "uMaterialDiffuseColor");
    uniforms.materialSpecularColor = glGetUniformLocation(m_shaderProgram, "uMaterialSpecularColor");
    uniforms.materialAmbientColor = glGetUniformLocation(m_shaderProgram, "uMaterialAmbientColor");
    uniforms.materialShininess = glGetUniformLocation(m_shaderProgram, "uMaterialShininess");
    uniforms.materialHasTexture = glGetUniformLocation(m_shaderProgram, "uMaterialHasTexture");

    uniforms.numLights = glGetUniformLocation(m_shaderProgram, "uNumLights");

    for (int i = 0; i < 8; i++) 
    {
        std::string prefix = "uLights[" + std::to_string(i) + "].";
        uniforms.lightTypes[i] = glGetUniformLocation(m_shaderProgram, (prefix + "type").c_str());
        uniforms.lightColors[i] = glGetUniformLocation(m_shaderProgram, (prefix + "color").c_str());
        uniforms.lightIntensities[i] = glGetUniformLocation(m_shaderProgram, (prefix + "intensity").c_str());
        uniforms.lightPositions[i] = glGetUniformLocation(m_shaderProgram, (prefix + "position").c_str());
        uniforms.lightDirections[i] = glGetUniformLocation(m_shaderProgram, (prefix + "direction").c_str());
        uniforms.lightConstants[i] = glGetUniformLocation(m_shaderProgram, (prefix + "constant").c_str());
        uniforms.lightLinears[i] = glGetUniformLocation(m_shaderProgram, (prefix + "linear").c_str());
        uniforms.lightQuadratics[i] = glGetUniformLocation(m_shaderProgram, (prefix + "quadratic").c_str());
        uniforms.lightCutOffs[i] = glGetUniformLocation(m_shaderProgram, (prefix + "cutOff").c_str());
        uniforms.lightOuterCutOffs[i] = glGetUniformLocation(m_shaderProgram, (prefix + "outerCutOff").c_str());
    }

    LOG_INFO("Shaders initialized successfully");
}

bool OpenGLRenderAdapter::checkShaderCompileErrors(unsigned int shader, const std::string& type)
{
    int success;
    char infoLog[1024];

    if (type != "PROGRAM") 
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) 
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            LOG_ERROR("Shader compilation error " + std::string(infoLog));
            return false;
        }
    }
    return true;
}

void OpenGLRenderAdapter::pollEvents()
{
    keyEvents.clear();
    mouseButtonEvents.clear();
    mouseMoveEvents.clear();
    mouseScrollEvents.clear();

    glfwPollEvents();
}

void OpenGLRenderAdapter::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (g_instance) 
    {
        g_instance->onKey(key, action);
    }
}

void OpenGLRenderAdapter::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (g_instance)
    {
        g_instance->onMouseButton(button, action);
    }
}

void OpenGLRenderAdapter::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (g_instance) 
    {
        g_instance->onMouseMove(xpos, ypos);
    }
}

void OpenGLRenderAdapter::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (g_instance)
    {
        g_instance->onMouseScroll(xoffset, yoffset);
    }
}
