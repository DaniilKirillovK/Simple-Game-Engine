#include "OpenGLRenderAdapter.h"

#include <filesystem>

#include "Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components/MeshRenderer.h"
#include "Components/Material.h"
#include "Components/Light.h"
#include "World.h"
#include "Components/Transform.h"
#include "glm/gtc/type_ptr.hpp"

#include "Resources/ShaderLoader/ShaderLoader.h"
#include "Resources/ShaderLoader/ShaderProgram.h"
#include "Resources/TextureLoader/Texture.h"
#include "Common/KeyCode.h"
#include "libs/imgui/imgui.h"
#include "libs/imgui/backends/imgui_impl_glfw.h"
#include "libs/imgui/backends/imgui_impl_opengl3.h"
#include "libs/imgui/imguizmo/ImGuizmo.h"
#include "Components/Tag.h"
#include "Components/Rigidbody.h"
#include "Components/Collider.h"

static const char* debugVertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec4 aColor;
    
    uniform mat4 uView;
    uniform mat4 uProjection;
    
    out vec4 vColor;
    
    void main() {
        gl_Position = uProjection * uView * vec4(aPos, 1.0);
        vColor = aColor;
    }
)";

static const char* debugFragmentShaderSource = R"(
    #version 330 core
    in vec4 vColor;
    out vec4 FragColor;
    
    void main() {
        FragColor = vColor;
    }
)";

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

    initImGui();

    setupOpenGLState();

    setupDebugBuffers();
    createDebugShader();

    std::string openGLVersion = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    std::string openGLRenderer = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    LOG_INFO("OpenGL version: " + openGLVersion);
    LOG_INFO("OpenGL renderer: " + openGLRenderer);

    return true;
}

void OpenGLRenderAdapter::render(World* world)
{
    endRenderToTexture();

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    beginImGuiFrame();
    ImGui::DockSpaceOverViewport();

    renderUI(world);
    
    endImGuiFrame();
    glfwSwapBuffers(m_window);

    resizeRenderTexture((int)m_viewportSize.x, (int)m_viewportSize.y);
    beginRenderToTexture();
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
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

    shutdownImGui();

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
    event.keyCode = static_cast<KeyCode>(key);

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

void OpenGLRenderAdapter::initImGui()
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    LOG_INFO("OpenGLRenderAdapter: ImGui initialized");
}

void OpenGLRenderAdapter::beginImGuiFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void OpenGLRenderAdapter::endImGuiFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void OpenGLRenderAdapter::shutdownImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    LOG_INFO("OpenGLRenderAdapter: ImGui shutdown");
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

void OpenGLRenderAdapter::beginDebugDraw()
{
    m_debugVertices.clear();
}

void OpenGLRenderAdapter::endDebugDraw()
{
    flushDebugDraw();
}

void OpenGLRenderAdapter::drawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
{
    DebugVertex v1{ start, color };
    DebugVertex v2{ end, color };
    m_debugVertices.push_back(v1);
    m_debugVertices.push_back(v2);
}

void OpenGLRenderAdapter::drawDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color)
{
    glm::vec3 corners[8] = 
    {
        {min.x, min.y, min.z}, {max.x, min.y, min.z},
        {max.x, min.y, max.z}, {min.x, min.y, max.z},
        {min.x, max.y, min.z}, {max.x, max.y, min.z},
        {max.x, max.y, max.z}, {min.x, max.y, max.z}
    };

    int edges[12][2] = 
    {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    for (const auto& edge : edges) 
    {
        drawDebugLine(corners[edge[0]], corners[edge[1]], color);
    }
}

void OpenGLRenderAdapter::drawDebugSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments)
{
    auto drawCircle = [&](const glm::vec3& axis1, const glm::vec3& axis2) {
        for (int i = 0; i < segments; ++i) 
        {
            float angle1 = glm::two_pi<float>() * i / segments;
            float angle2 = glm::two_pi<float>() * (i + 1) / segments;

            glm::vec3 p1 = center + (axis1 * cos(angle1) + axis2 * sin(angle1)) * radius;
            glm::vec3 p2 = center + (axis1 * cos(angle2) + axis2 * sin(angle2)) * radius;

            drawDebugLine(p1, p2, color);
        }
    };

    drawCircle(glm::vec3(1, 0, 0), glm::vec3(0, 1, 0));
    drawCircle(glm::vec3(1, 0, 0), glm::vec3(0, 0, 1));
    drawCircle(glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
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

void OpenGLRenderAdapter::createRenderTexture(int width, int height)
{
    if (width <= 0 || height <= 0) return;

    m_textureWidth = width;
    m_textureHeight = height;

    glGenTextures(1, &m_renderTexture);
    glBindTexture(GL_TEXTURE_2D, m_renderTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &m_depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);


    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_renderTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("Framebuffer is not complete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //LOG_INFO("Created render texture: %dx%d", width, height);
}

void OpenGLRenderAdapter::beginRenderToTexture()
{
    if (m_fbo == 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_textureWidth, m_textureHeight);
}

void OpenGLRenderAdapter::endRenderToTexture()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int windowWidth, windowHeight;
    glfwGetFramebufferSize(m_window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
}

void OpenGLRenderAdapter::resizeRenderTexture(int width, int height)
{
    if (width == m_textureWidth && height == m_textureHeight) return;

    if (m_renderTexture) glDeleteTextures(1, &m_renderTexture);
    if (m_depthBuffer) glDeleteRenderbuffers(1, &m_depthBuffer);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);

    createRenderTexture(width, height);
}

void OpenGLRenderAdapter::renderUI(World* world)
{
    renderUIViewport(world);
    renderSceneHierarchy(world);
    renderInspector(world);
    renderStatistics(world);
}

void OpenGLRenderAdapter::renderUIViewport(World* world)
{
    float aspectRatio = 16.0f / 9.0f;
    ImGui::Begin("Viewport", nullptr, 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar);

    m_viewportSize = ImGui::GetContentRegionAvail();
    m_viewportHovered = ImGui::IsWindowHovered();

    ImVec2 imagePos = ImGui::GetCursorScreenPos();
    ImVec2 imageSize;

    if (m_renderTexture != 0 && m_viewportSize.x > 0 && m_viewportSize.y > 0)
    {
        float targetWidth = m_viewportSize.y * aspectRatio;

        if (targetWidth <= m_viewportSize.x)
        {
            imageSize = ImVec2(targetWidth, m_viewportSize.y);
            float offsetX = (m_viewportSize.x - targetWidth) * 0.5f;
            imagePos.x += offsetX;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        }
        else
        {
            imageSize = ImVec2(m_viewportSize.x, m_viewportSize.x / aspectRatio);
            float offsetY = (m_viewportSize.y - imageSize.y) * 0.5f;
            imagePos.y += offsetY;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
        }

        ImGui::Image((ImTextureID)(intptr_t)m_renderTexture, imageSize,
            ImVec2(0, 1),
            ImVec2(1, 0));

        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

        if (m_selectedEntity != -1 && world)
        {
            Transform* entityTransform = world->getComponent<Transform>(m_selectedEntity);
            if (entityTransform)
            {
                glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), entityTransform->position);
                modelMatrix *= glm::mat4_cast(entityTransform->rotation);
                modelMatrix = glm::scale(modelMatrix, entityTransform->scale);

                ImGuizmo::OPERATION op = (m_gizmoOperation == 0 ? ImGuizmo::TRANSLATE : (m_gizmoOperation == 1 ? ImGuizmo::ROTATE : ImGuizmo::SCALE));
                ImGuizmo::MODE mode = m_gizmoLocalSpace ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

                ImGuizmo::Manipulate(glm::value_ptr(currentViewMatrix),
                    glm::value_ptr(currentProjectionMatrix),
                    op, mode,
                    glm::value_ptr(modelMatrix));

                if (ImGuizmo::IsUsing())
                {
                    glm::vec3 newPosition, newScale;
                    glm::quat newRotation;
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix),
                        glm::value_ptr(newPosition),
                        glm::value_ptr(newRotation),
                        glm::value_ptr(newScale));

                    entityTransform->position = newPosition;
                    entityTransform->rotation = newRotation;
                    entityTransform->scale = newScale;
                }
            }
        }

        ImVec2 toolbarPos = ImGui::GetWindowPos();
        ImVec2 toolbarSize = ImGui::GetContentRegionAvail();
        renderToolbar(toolbarPos, toolbarSize);
    }

    ImGui::End();
}

void OpenGLRenderAdapter::renderSceneHierarchy(World* world)
{
    ImGui::Begin("Scene Hierarchy");

    auto& transforms = world->getComponentPool<Transform>();

    for (auto& [entity, transform] : transforms.getAll())
    {
        std::string entityName = "Entity " + std::to_string(entity);

        if (world->hasComponent<Tag>(entity))
        {
            Tag* tag = world->getComponent<Tag>(entity);
            if (tag && !tag->name.empty())
            {
                entityName = tag->name;
            }
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
        if (m_selectedEntity == entity)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (ImGui::Selectable(entityName.c_str(), m_selectedEntity == entity))
        {
            m_selectedEntity = entity;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ID: %d", entity);
        }
    }

    ImGui::End();
}

void OpenGLRenderAdapter::renderInspector(World* world)
{
    ImGui::Begin("Inspector");

    if (m_selectedEntity == -1)
    {
        ImGui::Text("No entity selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity ID: %d", m_selectedEntity);
    ImGui::SameLine();

    ImGui::Separator();

    if (Transform* transform = world->getComponent<Transform>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            renderTransformEditor(*transform);
        }
    }

    if (Tag* tag = world->getComponent<Tag>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char nameBuffer[256];
            strncpy(nameBuffer, tag->name.c_str(), sizeof(nameBuffer));
            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                tag->name = nameBuffer;
            }
        }
    }
    else
    {
        if (ImGui::CollapsingHeader("+ Add Component"))
        {
            if (ImGui::MenuItem("Add Tag"))
            {
                world->addComponent<Tag>(m_selectedEntity, Tag{});
            }
        }
    }

    if (MeshRenderer* meshRenderer = world->getComponent<MeshRenderer>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Mesh Renderer"))
        {
            ImGui::Checkbox("Visible", &meshRenderer->visible);
        }
    }

    if (Rigidbody* rb = world->getComponent<Rigidbody>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Rigidbody"))
        {
            ImGui::Checkbox("Use Gravity", &rb->useGravity);
            ImGui::Checkbox("Is Kinematic", &rb->isKinematic);
            ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.01f, 1000.0f);
            if (ImGui::IsItemEdited()) rb->updateInvMass();
            ImGui::Text("Velocity: %.2f, %.2f, %.2f", rb->velocity.x, rb->velocity.y, rb->velocity.z);
        }
    }

    if (Collider* collider = world->getComponent<Collider>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Collider"))
        {
            const char* types[] = { "Box", "Sphere" };
            int currentType = (int)collider->type;
            if (ImGui::Combo("Type", &currentType, types, 2))
            {
                collider->type = (ColliderType)currentType;
            }

            if (collider->type == ColliderType::Box)
            {
                ImGui::DragFloat3("Half Size", glm::value_ptr(collider->halfSize), 0.1f);
            }
            else
            {
                ImGui::DragFloat("Radius", &collider->radius, 0.1f);
            }

            ImGui::DragFloat3("Offset", glm::value_ptr(collider->offset), 0.1f);
            ImGui::DragFloat("Bounciness", &collider->bounciness, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox("Is Trigger", &collider->isTrigger);
        }
    }

    ImGui::End();
}

void OpenGLRenderAdapter::renderStatistics(World* world)
{
    ImGui::Begin("Statistics");

    static float fpsHistory[100] = { 0 };
    static int fpsIndex = 0;

    float deltaTime = world->getCurrentDeltaTime();

    m_fpsCounter++;
    m_fpsAccumulator += deltaTime;
    if (m_fpsAccumulator >= 1.0f)
    {
        m_currentFPS = m_fpsCounter;
        m_fpsCounter = 0;
        m_fpsAccumulator = 0.0f;

        fpsHistory[fpsIndex] = m_currentFPS;
        fpsIndex = (fpsIndex + 1) % 100;
    }

    ImGui::Text("FPS: %d", m_currentFPS);
    ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);

    ImGui::Separator();
    if (world)
    {
        int totalEntities = 0;
        int meshRenderersCount = 0;
        int rigidbodiesCount = 0;

        auto& transforms = world->getComponentPool<Transform>();
        totalEntities = transforms.getAll().size();

        auto& meshRenderers = world->getComponentPool<MeshRenderer>();
        meshRenderersCount = meshRenderers.getAll().size();

        auto& rigidbodies = world->getComponentPool<Rigidbody>();
        rigidbodiesCount = rigidbodies.getAll().size();

        ImGui::Text("Total Entities: %d", totalEntities);
        ImGui::Text("Mesh Renderers: %d", meshRenderersCount);
        ImGui::Text("Rigidbodies: %d", rigidbodiesCount);
    }

    ImGui::Separator();

    ImGui::Text("FPS History:");
    ImGui::PlotLines("##FPS", fpsHistory, 100, fpsIndex, nullptr, 0.0f, 120.0f, ImVec2(0, 60));

    ImGui::Separator();

    ImGui::End();
}

void OpenGLRenderAdapter::renderAboutWindow()
{
}

void OpenGLRenderAdapter::renderTransformEditor(Transform& transform)
{
    ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.1f);

    glm::vec3 rotation = glm::degrees(glm::eulerAngles(transform.rotation));
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 1.0f))
    {
        transform.rotation = glm::quat(glm::radians(rotation));
    }

    ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.1f, 0.01f);
}

void OpenGLRenderAdapter::renderToolbar(ImVec2 position, ImVec2 size)
{
    ImGui::SetNextWindowPos(ImVec2(position.x, position.y + 20));
    ImGui::SetNextWindowSize(ImVec2(size.x, 40));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Toolbar", nullptr, flags))
    {
        ImGui::SameLine(10);

        if (ImGui::RadioButton("Move", m_gizmoOperation == 0))
        {
            m_gizmoOperation = 0;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Rotate", m_gizmoOperation == 1))
        {
            m_gizmoOperation = 1;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Scale", m_gizmoOperation == 2))
        {
            m_gizmoOperation = 2;
        }

        ImGui::SameLine(200);
        ImGui::Checkbox("Local", &m_gizmoLocalSpace);

        ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    }
    ImGui::End();
}

void OpenGLRenderAdapter::SizeCallback(ImGuiSizeCallbackData* data)
{
    float aspectRatio = *(float*)data->UserData;
    float newHeight = data->DesiredSize.x / aspectRatio;
    data->DesiredSize.y = newHeight;
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

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    m_window = glfwCreateWindow(mode->width, mode->height, "OpenGL Window", nullptr, nullptr);
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

void OpenGLRenderAdapter::setupDebugBuffers()
{
    glGenVertexArrays(1, &m_debugVAO);
    glGenBuffers(1, &m_debugVBO);

    glBindVertexArray(m_debugVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugVBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, color));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void OpenGLRenderAdapter::flushDebugDraw()
{
    if (m_debugVertices.empty()) return;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    glUseProgram(m_debugShaderProgram);

    int viewLoc = glGetUniformLocation(m_debugShaderProgram, "uView");
    int projLoc = glGetUniformLocation(m_debugShaderProgram, "uProjection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(currentViewMatrix));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(currentProjectionMatrix));

    glBindBuffer(GL_ARRAY_BUFFER, m_debugVBO);
    glBufferData(GL_ARRAY_BUFFER, m_debugVertices.size() * sizeof(DebugVertex),
        m_debugVertices.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(m_debugVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_debugVertices.size()));

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void OpenGLRenderAdapter::createDebugShader()
{
    auto compileShader = [](const char* source, GLenum type) -> unsigned int {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) 
        {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            LOG_INFO("Debug shader compilation error: " + std::string(infoLog));
        }
        return shader;
    };

    unsigned int vertexShader = compileShader(debugVertexShaderSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(debugFragmentShaderSource, GL_FRAGMENT_SHADER);

    m_debugShaderProgram = glCreateProgram();
    glAttachShader(m_debugShaderProgram, vertexShader);
    glAttachShader(m_debugShaderProgram, fragmentShader);
    glLinkProgram(m_debugShaderProgram);

    int success;
    glGetProgramiv(m_debugShaderProgram, GL_LINK_STATUS, &success);
    if (!success) 
    {
        char infoLog[512];
        glGetProgramInfoLog(m_debugShaderProgram, 512, nullptr, infoLog);
        LOG_INFO("Debug shader linking error: " + std::string(infoLog));
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
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
            LOG_ERROR("Shader compilation error: " + std::string(infoLog));
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
