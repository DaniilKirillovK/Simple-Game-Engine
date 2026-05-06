#include "OpenGLRenderAdapter.h"

#include <filesystem>

#include "Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components/MeshRenderer.h"
#include "Components/Material.h"
#include "Components/Light.h"
#include "Components/Transform.h"
#include <glm/gtc/type_ptr.hpp>

#include "Resources/ShaderLoader/ShaderLoader.h"
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "Resources/TextureLoader/Texture.h"

static OpenGLRenderAdapter *g_instance = nullptr;

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

    std::string openGLVersion = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    std::string openGLRenderer = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    LOG_INFO("OpenGL version: " + openGLVersion);
    LOG_INFO("OpenGL renderer: " + openGLRenderer);

    return true;
}

void OpenGLRenderAdapter::render()
{
    glfwSwapBuffers(m_window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderAdapter::shutdown()
{
    LOG_INFO("OpenGL shutting down");

    // Clear resources
    for (auto &[mesh, vao]: meshVAOs)
    {
        glDeleteVertexArrays(1, &vao);
    }
    for (auto &[mesh, vbo]: meshVBOs)
    {
        glDeleteBuffers(1, &vbo);
    }
    for (auto &[mesh, ebo]: meshEBOs)
    {
        glDeleteBuffers(1, &ebo);
    }
    meshVAOs.clear();
    meshVBOs.clear();
    meshEBOs.clear();

    if (m_shaderProgram->programId)
    {
        glDeleteProgram(m_shaderProgram->programId);
    }

    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void OpenGLRenderAdapter::framebufferSizeCallback(GLFWwindow *window, int width, int height)
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

uint32_t OpenGLRenderAdapter::createTexture(const TextureData& data)
{
    if (!data.pixels || data.width <= 0 || data.height <= 0) 
    {
        LOG_ERROR("OpenGLRenderAdapter: Invalid texture data");
        return 0;
    }

    uint32_t textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, data.getInternalFormat(),
        data.width, data.height, 0,
        data.getPixelFormat(), GL_UNSIGNED_BYTE, data.pixels);

    glGenerateMipmap(GL_TEXTURE_2D);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) 
    {
        LOG_ERROR("OpenGLRenderAdapter: OpenGL error while creating texture: " + error);
        glDeleteTextures(1, &textureID);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    LOG_INFO("OpenGLRenderAdapter: Created texture with ID=" + std::to_string(textureID) + " | Size: " + std::to_string(data.width) + "x" + std::to_string(data.height));

    return textureID;
}

void OpenGLRenderAdapter::destroyTexture(uint32_t handle)
{
    if (handle != 0) 
    {
        GLuint textureID = handle;
        glDeleteTextures(1, &textureID);
    }
}

void OpenGLRenderAdapter::setModelMatrix(const float *matrix)
{
    currentModelMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(m_shaderProgram->uniforms.modelMatrix, 1, GL_FALSE, matrix);
}

void OpenGLRenderAdapter::setViewMatrix(const float *matrix)
{
    currentViewMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(m_shaderProgram->uniforms.viewMatrix, 1, GL_FALSE, matrix);
}

void OpenGLRenderAdapter::setProjectionMatrix(const float *matrix)
{
    currentProjectionMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(m_shaderProgram->uniforms.projectionMatrix, 1, GL_FALSE, matrix);
}

void OpenGLRenderAdapter::setNormalMatrix(const float *matrix)
{
    currentNormalMatrix = glm::make_mat4(matrix);
    glUniformMatrix4fv(m_shaderProgram->uniforms.normalMatrix, 1, GL_FALSE, matrix);
}

float OpenGLRenderAdapter::getAspectRatio()
{
    return (float) m_width / m_height;
}

void OpenGLRenderAdapter::setShaderProgram(const ShaderProgram* shaderProgram)
{
    m_shaderProgram = shaderProgram;
    glUseProgram(m_shaderProgram->programId);
}

void OpenGLRenderAdapter::setMaterial(const Material *material)
{
    currentMaterial = material;

    if (m_shaderProgram->uniforms.materialDiffuseColor != -1)
    {
        glUniform4f(m_shaderProgram->uniforms.materialDiffuseColor,
                    material->diffuseColor.r, material->diffuseColor.g,
                    material->diffuseColor.b, material->diffuseColor.a);
    }

    if (m_shaderProgram->uniforms.materialSpecularColor != -1)
    {
        glUniform4f(m_shaderProgram->uniforms.materialSpecularColor,
                    material->specularColor.r, material->specularColor.g,
                    material->specularColor.b, material->specularColor.a);
    }

    if (m_shaderProgram->uniforms.materialAmbientColor != -1)
    {
        glUniform4f(m_shaderProgram->uniforms.materialAmbientColor,
                    material->ambientColor.r, material->ambientColor.g,
                    material->ambientColor.b, material->ambientColor.a);
    }

    if (m_shaderProgram->uniforms.materialShininess != -1)
    {
        glUniform1f(m_shaderProgram->uniforms.materialShininess, material->shininess);
    }

    if (m_shaderProgram->uniforms.materialHasTexture != -1)
    {
        glUniform1i(m_shaderProgram->uniforms.materialHasTexture, material->hasTexture ? 1 : 0);
    }

    if (material->hasTexture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material->diffuseTexture->m_handle);
        glUniform1i(m_shaderProgram->uniforms.texture, 0);
    }
}

void OpenGLRenderAdapter::setLights(const std::vector<Light *> &lights)
{
    currentLights = lights;

    int numLights = std::min((int) lights.size(), 8);
    glUniform1i(m_shaderProgram->uniforms.numLights, numLights);

    for (int i = 0; i < numLights; i++)
    {
        const Light *light = lights[i];

        if (m_shaderProgram->uniforms.lightTypes[i] != -1)
        {
            glUniform1i(m_shaderProgram->uniforms.lightTypes[i], static_cast<int>(light->type));
        }

        if (m_shaderProgram->uniforms.lightColors[i] != -1)
        {
            glUniform3f(m_shaderProgram->uniforms.lightColors[i], light->color.r, light->color.g, light->color.b);
        }

        if (m_shaderProgram->uniforms.lightIntensities[i] != -1)
        {
            glUniform1f(m_shaderProgram->uniforms.lightIntensities[i], light->intensity);
        }

        if (m_shaderProgram->uniforms.lightPositions[i] != -1)
        {
            glUniform3f(m_shaderProgram->uniforms.lightPositions[i], light->position.x, light->position.y, light->position.z);
        }

        if (m_shaderProgram->uniforms.lightDirections[i] != -1)
        {
            glUniform3f(m_shaderProgram->uniforms.lightDirections[i], light->direction.x, light->direction.y, light->direction.z);
        }

        if (m_shaderProgram->uniforms.lightConstants[i] != -1)
        {
            glUniform1f(m_shaderProgram->uniforms.lightConstants[i], light->constant);
        }

        if (m_shaderProgram->uniforms.lightLinears[i] != -1)
        {
            glUniform1f(m_shaderProgram->uniforms.lightLinears[i], light->linear);
        }

        if (m_shaderProgram->uniforms.lightQuadratics[i] != -1)
        {
            glUniform1f(m_shaderProgram->uniforms.lightQuadratics[i], light->quadratic);
        }

        if (m_shaderProgram->uniforms.lightCutOffs[i] != -1)
        {
            glUniform1f(m_shaderProgram->uniforms.lightCutOffs[i], light->cutOff);
        }

        if (m_shaderProgram->uniforms.lightOuterCutOffs[i] != -1)
        {
            glUniform1f(m_shaderProgram->uniforms.lightOuterCutOffs[i], light->outerCutOff);
        }
    }
}

void OpenGLRenderAdapter::drawMesh(const Mesh *mesh)
{
    if (!mesh)
        return;

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

    glm::mat4 viewMatrix = currentViewMatrix;
    glm::vec3 cameraPosition = glm::vec3(
        -viewMatrix[3][0] * viewMatrix[0][0] - viewMatrix[3][1] * viewMatrix[1][0] - viewMatrix[3][2] * viewMatrix[2][
            0],
            -viewMatrix[3][0] * viewMatrix[0][1] - viewMatrix[3][1] * viewMatrix[1][1] - viewMatrix[3][2] * viewMatrix[2][
                1],
                -viewMatrix[3][0] * viewMatrix[0][2] - viewMatrix[3][1] * viewMatrix[1][2] - viewMatrix[3][2] * viewMatrix[2][2]
                );

    GLint cameraPosLoc = m_shaderProgram->uniforms.cameraPosition;
    if (cameraPosLoc != -1)
    {
        glUniform3f(cameraPosLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);
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
    return gladLoadGLLoader((GLADloadproc) glfwGetProcAddress) != 0;
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

void OpenGLRenderAdapter::updateProjectionMatrix()
{
}

void OpenGLRenderAdapter::createMeshVAO(Mesh *mesh)
{
    if (!mesh)
        return;

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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *) (6 * sizeof(float)));
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
    
}

bool OpenGLRenderAdapter::checkShaderCompileErrors(unsigned int shader, const std::string &type)
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

void OpenGLRenderAdapter::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (g_instance)
    {
        g_instance->onKey(key, action);
    }
}

void OpenGLRenderAdapter::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (g_instance)
    {
        g_instance->onMouseButton(button, action);
    }
}

void OpenGLRenderAdapter::cursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    if (g_instance)
    {
        g_instance->onMouseMove(xpos, ypos);
    }
}

void OpenGLRenderAdapter::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (g_instance)
    {
        g_instance->onMouseScroll(xoffset, yoffset);
    }
}

unsigned int OpenGLRenderAdapter::compileShaderSource(const std::string& source, ShaderType type)
{
    GLenum glType = GL_VERTEX_SHADER;

    if (type == ShaderType::Fragment)
    {
        glType = GL_FRAGMENT_SHADER;
    }

    GLuint shader = glCreateShader(glType);

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);

        LOG_ERROR("OpenGL shader compile error:\n" + std::string(infoLog));

        glDeleteShader(shader);
        return 0;
    }

    LOG_INFO("Shader compiled successfully");

    return shader;
}

unsigned int OpenGLRenderAdapter::linkShaderProgram(unsigned int vertexShader, unsigned int fragmentShader)
{
    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);

        LOG_ERROR("OpenGL shader link error:\n" + std::string(infoLog));

        glDeleteProgram(program);
        return 0;
    }

    LOG_INFO("Shader program linked successfully");

    return program;
}

void OpenGLRenderAdapter::deleteShaderObject(unsigned int shader)
{
    glDeleteShader(shader);
}

void OpenGLRenderAdapter::deleteShaderProgram(unsigned int program)
{
    glDeleteProgram(program);
}
