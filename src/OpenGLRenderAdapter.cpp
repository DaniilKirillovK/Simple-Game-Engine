#include "OpenGLRenderAdapter.h"
#include "Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static OpenGLRenderAdapter* g_instance = nullptr;

// Vertex Shader
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uProjection;

out vec3 ourColor;

void main() 
{
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    ourColor = aColor;
}
)";

// Pixel Shader
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 ourColor;

void main() 
{
    FragColor = vec4(ourColor, 1.0);
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
    setupSquare();

    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    m_projectionLoc = glGetUniformLocation(m_shaderProgram, "uProjection");
    updateProjection();

    std::string openGLVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::string openGLRenderer = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    LOG_INFO("OpenGL version: " + openGLVersion);
    LOG_INFO("OpenGL renderer: " + openGLRenderer);

    return true;
}

void OpenGLRenderAdapter::render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSquare();

    glfwSwapBuffers(m_window);
    glfwPollEvents();
}

void OpenGLRenderAdapter::shutdown()
{
    LOG_INFO("OpenGL shutting down");

    // Clear resources
    if (m_VAO) 
    {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
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

bool OpenGLRenderAdapter::initGLFW(int width, int height)
{
    if (!glfwInit()) 
    {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(width, height, "OpenGL Window", nullptr, nullptr);
    if (!m_window) 
    {
        return false;
    }

    glfwMakeContextCurrent(m_window);
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
    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    glViewport(0, 0, width, height);
    updateProjection();
}

void OpenGLRenderAdapter::setupSquare()
{
    float vertices[] = 
    {
        -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  1.0f, 1.0f, 0.0f
    };

    unsigned int indices[] = 
    {
        0, 1, 2,
        1, 3, 2    
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
        5 * sizeof(float),
        (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void OpenGLRenderAdapter::drawSquare()
{
    glUseProgram(m_shaderProgram);

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void OpenGLRenderAdapter::updateProjection()
{
    glm::mat4 projection = glm::ortho(
        -1.0f * m_aspectRatio,
        1.0f * m_aspectRatio,
        -1.0f,
        1.0f,
        -1.0f,
        1.0f
    );

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_projectionLoc, 1, GL_FALSE, &projection[0][0]);
}

bool OpenGLRenderAdapter::shouldClose() const
{
    return glfwWindowShouldClose(m_window);
}

void OpenGLRenderAdapter::compileShaders()
{
    // Vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    if (!checkShaderCompileErrors(vertexShader, "VERTEX")) return;

    // Fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
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
