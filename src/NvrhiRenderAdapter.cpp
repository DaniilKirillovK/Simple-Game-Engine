#include "NvrhiRenderAdapter.h"

#include <cmath>
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

#include "Resources/ShaderLoader/ShaderProgram.h"
#include "Resources/TextureLoader/Texture.h"
#include "tracy/Tracy.hpp"
#include "Resources/AssetLoader.h"
#include "Common/KeyCode.h"
#include "libs/imgui/imgui.h"
#include "libs/imgui/backends/imgui_impl_glfw.h"
#include "GpuRenderer.h"
#include "libs/imgui/imguizmo/ImGuizmo.h"
#include "Components/Tag.h"
#include "Components/Rigidbody.h"
#include "Components/Collider.h"
#include "Components/Script.h"
#include "Scripting/ScriptInspector.h"

#include "Utils/HierarchyUtils.h"
#include "Utils/RayCast.h"
#include "Utils/Serialization/SceneSerializer.h"
#include "libs/imgui/imgui_internal.h"
#include <Resources/ResourceManager.h>
#include "MeshFactory.h"
#include "Gameplay/PrefabLibrary.h"
#include "Components/PrefabInstance.h"
#include "Systems/ScriptSystem.h"
#include "Scripting/ScriptConsole.h"
#include <optional>

static NvrhiRenderAdapter *g_instance = nullptr;

NvrhiRenderAdapter::NvrhiRenderAdapter(bool vulkan, bool verify)
    : gpu(std::make_unique<GpuRenderer>(vulkan, verify)), m_window(nullptr)
      , m_width(DEFAULT_WIDTH)
      , m_height(DEFAULT_HEIGHT)
      , m_shaderProgram(nullptr)
      , m_world(nullptr)
{
    m_autoSaveEnabled = !verify;
    g_instance = this;
}

NvrhiRenderAdapter::~NvrhiRenderAdapter()
{
    shutdown();
}

bool NvrhiRenderAdapter::initialize(int width, int height)
{
    m_width = width;
    m_height = height;

    m_window = gpu->initialize(width, height);
    if (!m_window)
    {
        return false;
    }

    glfwGetFramebufferSize(m_window, &m_width, &m_height);
    m_sceneImageSize = ImVec2(float(std::max(1, m_width)), float(std::max(1, m_height)));

    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

    initImGui();

    Mesh::gpuReleaseContext = this;
    Mesh::gpuReleaseHook = [](void* ctx, Mesh* mesh) {
        static_cast<NvrhiRenderAdapter*>(ctx)->gpu->releaseMesh(mesh);
    };
    return true;
}

bool NvrhiRenderAdapter::initializeHeadless(int width, int height)
{
    m_width = width;
    m_height = height;
    m_headless = true;

    if (!gpu->initializeHeadless(width, height))
    {
        return false;
    }

    m_sceneImageSize = ImVec2(float(width), float(height));

    Mesh::gpuReleaseContext = this;
    Mesh::gpuReleaseHook = [](void* ctx, Mesh* mesh) {
        static_cast<NvrhiRenderAdapter*>(ctx)->gpu->releaseMesh(mesh);
    };
    return true;
}

void NvrhiRenderAdapter::runFramesHeadless(const std::function<void()>& frame, uint32_t frameCount)
{
    gpu->runHeadless(frame, frameCount);
}

bool NvrhiRenderAdapter::readSceneToMemory(std::vector<uint8_t>& rgb, uint32_t& width, uint32_t& height)
{
    return gpu->readSceneToMemory(rgb, width, height);
}

void NvrhiRenderAdapter::render(World* world)
{
    m_world = world;

    if (m_headless)
    {
        // No window, no UI: the world update already submitted the scene.
        return;
    }

    if (m_stopRequested)
    {
        m_stopRequested = false;
        if (m_isPlaying)
        {
            m_isPlaying = false;
            if (m_onStopCallback) m_onStopCallback();
        }
    }

    if (m_consoleToggleRequested)
    {
        m_consoleToggleRequested = false;
        toggleScriptConsoleFocus();
    }

    applyCursorLock();

    beginImGuiFrame();
    ImGui::DockSpaceOverViewport();

    renderUI();
    
    endImGuiFrame();
}

void NvrhiRenderAdapter::runFrames(const std::function<void()>& frame)
{
    gpu->run(frame);
}

void NvrhiRenderAdapter::shutdown()
{
    if (!m_window && !m_headless) return;

    if (m_autoSaveEnabled && m_world) saveCurrentScene();

    Mesh::gpuReleaseContext = nullptr;
    Mesh::gpuReleaseHook = nullptr;

    Logger::instance().setLogCallback({});
    if (!m_headless)
    {
        shutdownImGui();
    }

    gpu.reset();
    g_instance = nullptr;
    m_window = nullptr;
    m_headless = false;
}

void NvrhiRenderAdapter::framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    if (g_instance)
    {
        LOG_INFO("Frame buffer resize");
        g_instance->onFramebufferResize(width, height);
    }
}

void NvrhiRenderAdapter::onKey(int key, int action)
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

    if (event.pressed && key == GLFW_KEY_F1)
    {
        m_consoleToggleRequested = true;
        return;
    }

    if (event.pressed && ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput)
    {
        return;
    }

    if (event.pressed && key == GLFW_KEY_ESCAPE && m_isPlaying)
    {
        m_stopRequested = true;
    }

    keyEvents.push_back(event);
}

void NvrhiRenderAdapter::onMouseButton(int button, int action)
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

    if (event.pressed && !InputHandler::getInstance().isCursorLocked() && !m_viewportHovered)
    {
        return;
    }

    mouseButtonEvents.push_back(event);
}

void NvrhiRenderAdapter::onMouseMove(double x, double y)
{
    MouseMoveEvent event;
    event.x = x;
    event.y = y;
    mouseMoveEvents.push_back(event);
}

void NvrhiRenderAdapter::onMouseScroll(double xoffset, double yoffset)
{
    MouseScrollEvent event;
    event.xOffset = xoffset;
    event.yOffset = yoffset;
    mouseScrollEvents.push_back(event);
}

uint32_t NvrhiRenderAdapter::createTexture(const TextureData& data)
{
    return gpu->uploadTexture(data);
}

void NvrhiRenderAdapter::destroyTexture(uint32_t handle)
{
    gpu->destroyTexture(handle);
}

void NvrhiRenderAdapter::initImGui()
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;


    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(m_window, true);
    gpu->initGui();

    Logger::instance().setLogCallback([](LogLevel level, const std::string& message) {
        if (g_instance) 
        {
            g_instance->addLogEntry(level, message);
        }
    });

    AssetsUtils::scanAssetsDirectory("assets", m_currentDirectory, m_currentDirectoryContent);

    LOG_INFO("NvrhiRenderAdapter: ImGui initialized");
}

void NvrhiRenderAdapter::beginImGuiFrame()
{

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void NvrhiRenderAdapter::endImGuiFrame()
{
    ImGui::Render();

    gpu->drawGui(*ImGui::GetDrawData());

    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();

    gpu->resizeScene(int(m_sceneImageSize.x), int(m_sceneImageSize.y));
}

void NvrhiRenderAdapter::shutdownImGui()
{
    ImGui::DestroyPlatformWindows();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    LOG_INFO("NvrhiRenderAdapter: ImGui shutdown");
}

void NvrhiRenderAdapter::setModelMatrix(const float *matrix)
{
    currentModelMatrix = glm::make_mat4(matrix);
}

void NvrhiRenderAdapter::setViewMatrix(const float *matrix)
{
    currentViewMatrix = glm::make_mat4(matrix);
}

void NvrhiRenderAdapter::setProjectionMatrix(const float *matrix)
{
    currentProjectionMatrix = glm::make_mat4(matrix);
}

void NvrhiRenderAdapter::setNormalMatrix(const float *matrix)
{
    currentNormalMatrix = glm::make_mat4(matrix);
}

float NvrhiRenderAdapter::getAspectRatio()
{
    if (m_sceneImageSize.y > 0)
        return m_sceneImageSize.x / m_sceneImageSize.y;
    return float(m_width) / std::max(1, m_height);
}

void NvrhiRenderAdapter::setShaderProgram(const ShaderProgram* shaderProgram)
{
    m_shaderProgram = shaderProgram;

}

void NvrhiRenderAdapter::setMaterial(const Material *material)
{
    currentMaterial = material;

}

void NvrhiRenderAdapter::setLights(const std::vector<Light*>& lights)
{
    currentLights = lights;
}

void NvrhiRenderAdapter::beginDebugDraw()
{
    m_debugVertices.clear();
}

void NvrhiRenderAdapter::endDebugDraw()
{
    flushDebugDraw();
}

void NvrhiRenderAdapter::drawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
{
    DebugVertex v1{ start, color };
    DebugVertex v2{ end, color };
    m_debugVertices.push_back(v1);
    m_debugVertices.push_back(v2);
}

void NvrhiRenderAdapter::drawDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color)
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

void NvrhiRenderAdapter::drawDebugSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments)
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

void NvrhiRenderAdapter::drawMesh(const Mesh *mesh)
{
    if (!mesh || !mesh->ready)
        return;

    if (m_shaderProgram && currentMaterial)
    {
        gpu->drawMesh(
            *mesh, 
            *m_shaderProgram, 
            *currentMaterial, 
            currentLights,
            currentModelMatrix, 
            currentViewMatrix, 
            currentProjectionMatrix, 
            currentNormalMatrix
        );
    }
}

void NvrhiRenderAdapter::loadAssetIcons()
{
    auto folderIconResource = RESOURCE_MANAGER.load<Texture>("assets\\common\\icon_folder.png");
    auto modelIconResource = RESOURCE_MANAGER.load<Texture>("assets\\common\\icon_model.png");

    if (folderIconResource && folderIconResource->get())
        m_folderIcon = (ImTextureID)(intptr_t)folderIconResource->get()->m_handle;
    if (modelIconResource && modelIconResource->get())
        m_modelIcon = (ImTextureID)(intptr_t)modelIconResource->get()->m_handle;
}

void NvrhiRenderAdapter::toggleScriptConsoleFocus()
{
    InputHandler& input = InputHandler::getInstance();

    if (m_scriptConsoleFocused)
    {
        m_scriptConsoleFocusChange = -1;
        if (m_relockCursorAfterConsole && m_isPlaying)
        {
            input.setCursorLocked(true);
        }
        m_relockCursorAfterConsole = false;
        return;
    }

    m_relockCursorAfterConsole = input.isCursorLocked();
    input.setCursorLocked(false);
    m_showScriptConsole = true;
    m_scriptConsoleFocusChange = 1;
}

void NvrhiRenderAdapter::renderUI()
{
    if (m_scriptConsoleFocusChange < 0)
    {
        ImGui::ClearActiveID();
        ImGui::SetWindowFocus(nullptr);
        m_scriptConsoleFocusChange = 0;
    }

    renderMainMenuBar();

    if (m_showViewport) renderUIViewport();
    if (m_showHierarchy) renderSceneHierarchy();
    if (m_showInspector) renderInspector();
    if (m_showStatistics) renderStatistics();
    if (m_showLogPanel) renderLogPanel();
    if (m_showScriptConsole) renderScriptConsole();
    if (m_showAssetBrowser) renderAssetBrowser();
    if (m_showPrefabs) renderPrefabPanel();

    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        requestDeleteEntity(m_selectedEntity);
    }

    applyPendingDelete();
}

bool NvrhiRenderAdapter::canDeleteEntity(EntityId entity) const
{
    // Play mode restores the scene on Stop, and scripts may hold the entity
    return !m_isPlaying && m_world && entity != INVALID_ENTITY && m_world->isValidEntity(entity);
}

void NvrhiRenderAdapter::requestDeleteEntity(EntityId entity)
{
    if (canDeleteEntity(entity))
    {
        m_pendingDeleteEntity = entity;
    }
}

void NvrhiRenderAdapter::applyPendingDelete()
{
    const EntityId entity = m_pendingDeleteEntity;
    m_pendingDeleteEntity = INVALID_ENTITY;

    if (!canDeleteEntity(entity)) return;

    std::string name = "Entity " + std::to_string(entity);
    if (Tag* tag = m_world->getComponent<Tag>(entity))
    {
        if (!tag->name.empty()) name = tag->name;
    }

    HierarchyUtils::destroyRecursive(m_world, entity);

    // The selection may have been the entity or one of its children; -1 is "nothing selected" in the editor
    if (!m_world->isValidEntity(m_selectedEntity))
    {
        m_selectedEntity = -1;
    }
    if (!m_world->isValidEntity(m_prefabSaveNameEntity))
    {
        m_prefabSaveNameEntity = INVALID_ENTITY;
    }

    LOG_INFO("Deleted entity: " + name + " (id: " + std::to_string(entity) + ")");
}

void NvrhiRenderAdapter::renderPrefabFieldsEditor()
{
    // Follow the file until the user starts editing, then keep the local copy
    if (!m_editedPrefabDirty)
    {
        std::optional<Script> script = m_prefabLibrary->getScript(m_editedPrefab);
        if (!script)
        {
            ImGui::TextDisabled("'%s' has no script or failed to load (see log)", m_editedPrefab.c_str());
            return;
        }
        m_editedPrefabScript = std::move(*script);
    }

    ImGui::Text("Prefab fields: %s", m_editedPrefab.c_str());
    ImGui::TextDisabled("%s.%s", m_editedPrefabScript.moduleName.c_str(), m_editedPrefabScript.className.c_str());

    ImGui::PushID("PrefabFields");
    ScriptInspector::DrawOptions options;
    options.allowEntityReferences = false;
    if (ScriptInspector::drawFields(m_editedPrefabScript, *m_world, options))
    {
        m_editedPrefabDirty = true;
    }
    ImGui::PopID();

    ImGui::BeginDisabled(!m_editedPrefabDirty);
    if (ImGui::Button("Save to prefab"))
    {
        if (m_prefabLibrary->saveScriptFields(m_editedPrefab, m_editedPrefabScript.fields))
        {
            m_editedPrefabDirty = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard"))
    {
        m_editedPrefabDirty = false;
    }
    ImGui::EndDisabled();

    if (m_editedPrefabDirty)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "unsaved");
    }
    else if (m_isPlaying)
    {
        ImGui::TextDisabled("Saved values apply to the next spawn");
    }
}

void NvrhiRenderAdapter::renderPrefabPanel()
{
    if (!ImGui::Begin("Prefabs", &m_showPrefabs) || !m_prefabLibrary || !m_world)
    {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("%s", m_prefabLibrary->getDirectory().string().c_str());

    if (ImGui::Button("Refresh") || m_prefabListDirty)
    {
        m_prefabNames = m_prefabLibrary->list();
        m_prefabListDirty = false;
    }

    ImGui::Separator();
    if (m_prefabNames.empty())
    {
        ImGui::TextWrapped("No *.prefab.json files yet.");
    }

    for (const std::string& name : m_prefabNames)
    {
        ImGui::PushID(name.c_str());

        // Instances created here are scene entities; during Play only scripts spawn
        ImGui::BeginDisabled(m_isPlaying);
        if (ImGui::SmallButton("Add to scene"))
        {
            EntityId entity = m_prefabLibrary->instantiate(*m_world, name, glm::vec3(0.0f, 1.0f, 0.0f));
            if (entity != INVALID_ENTITY) m_selectedEntity = entity;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Template data may change during Play too: it only affects the next spawn
        if (ImGui::Selectable(name.c_str(), m_editedPrefab == name))
        {
            if (m_editedPrefab != name)
            {
                m_editedPrefab = name;
                m_editedPrefabDirty = false;
            }
        }
        ImGui::PopID();
    }

    if (!m_editedPrefab.empty())
    {
        ImGui::Separator();
        renderPrefabFieldsEditor();
    }

    ImGui::Separator();

    ImGui::BeginDisabled(m_isPlaying);

    const bool hasSelection = m_selectedEntity != INVALID_ENTITY && m_world->isValidEntity(m_selectedEntity);
    if (hasSelection)
    {
        // Suggest the selected entity's name once per selection
        if (m_prefabSaveNameEntity != m_selectedEntity)
        {
            m_prefabSaveNameEntity = m_selectedEntity;
            std::string suggested;
            if (Tag* tag = m_world->getComponent<Tag>(m_selectedEntity))
            {
                for (char c : tag->name)
                {
                    suggested += std::isalnum(static_cast<unsigned char>(c)) || c == '-' ? c : '_';
                }
            }
            snprintf(m_prefabSaveName, sizeof(m_prefabSaveName), "%s", suggested.c_str());
        }

        ImGui::InputText("Name", m_prefabSaveName, sizeof(m_prefabSaveName));

        const bool validName = PrefabLibrary::isValidName(m_prefabSaveName);
        ImGui::BeginDisabled(!validName);
        if (ImGui::Button("Save selected as prefab"))
        {
            if (m_prefabLibrary->saveFromEntity(*m_world, m_selectedEntity, m_prefabSaveName))
            {
                m_prefabListDirty = true;
            }
        }
        ImGui::EndDisabled();

        if (!validName)
        {
            ImGui::TextDisabled("Name: letters, digits, '_' and '-'");
        }
    }
    else
    {
        ImGui::TextDisabled("Select an entity to save it as a prefab");
    }

    ImGui::EndDisabled();

    if (m_isPlaying)
    {
        ImGui::TextWrapped("Stop Play mode to add prefabs to the scene or save new ones.");
    }

    ImGui::End();
}

void NvrhiRenderAdapter::renderUIViewport()
{
    if (!ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoTitleBar))
    {
        m_viewportHovered = false;
        ImGui::End();
        return;
    }

    m_viewportSize = ImGui::GetContentRegionAvail();
    m_viewportHovered = ImGui::IsWindowHovered();

    ImVec2 imagePos = ImGui::GetCursorScreenPos();
    const ImVec2 imageSize = m_viewportSize;

    if (gpu->sceneTexture() != 0 && m_viewportSize.x > 0 && m_viewportSize.y > 0)
    {
        auto* host = static_cast<GLFWwindow*>(ImGui::GetWindowViewport()->PlatformHandle);

        if (host)
        {
            int width, height, pixelWidth, pixelHeight;
            glfwGetWindowSize(host, &width, &height);
            glfwGetFramebufferSize(host, &pixelWidth, &pixelHeight);
            if (width > 0 && height > 0 && pixelWidth > 0 && pixelHeight > 0)
            {
                m_sceneImageSize.x = std::max(1.f, std::round(imageSize.x * pixelWidth / width));
                m_sceneImageSize.y = std::max(1.f, std::round(imageSize.y * pixelHeight / height));
            }
        }

        ImGui::Image((ImTextureID)(intptr_t)gpu->sceneTexture(), imageSize,
            ImVec2(0, 0),
            ImVec2(1, 1));

        if (ScriptSystem* scripts = ScriptEngine::getInstance().getActiveSystem())
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const float flash = scripts->getScreenFlashAlpha();
            if (flash > 0.001f)
            {
                drawScreenFlash(scripts->getScreenFlashColor(), flash, imagePos, imageSize);
            }

            float hudTop = imagePos.y + 10.0f;
            if (!m_cursorLockApplied)
            {
                hudTop = std::max(hudTop, ImGui::GetWindowPos().y + TOOLBAR_OFFSET + TOOLBAR_HEIGHT + 8.0f);
            }
            ImVec2 textPos(imagePos.x + 12.0f, hudTop);

            for (const auto& [key, text] : scripts->getHud())
            {
                const ImVec2 size = ImGui::CalcTextSize(text.c_str());
                drawList->AddRectFilled(ImVec2(textPos.x - 4.0f, textPos.y - 2.0f),
                    ImVec2(textPos.x + size.x + 4.0f, textPos.y + size.y + 2.0f), IM_COL32(0, 0, 0, 150), 3.0f);
                drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text.c_str());
                textPos.y += lineHeight;
            }

            if (!scripts->getHudCenter().empty())
            {
                drawCenterMessage(scripts->getHudCenter(), imagePos, imageSize);
            }
        }

        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

        if (m_selectedEntity != -1 && m_world)
        {
            Transform* entityTransform = m_world->getComponent<Transform>(m_selectedEntity);
            EntityId parentId = HierarchyUtils::getParent(m_world, m_selectedEntity);
            Transform* parentTransform = (parentId != -1) ? m_world->getComponent<Transform>(parentId) : nullptr;

            if (entityTransform)
            {
                glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), entityTransform->getWorldPosition());
                modelMatrix *= glm::mat4_cast(entityTransform->rotation);
                modelMatrix = glm::scale(modelMatrix, entityTransform->scale);

                ImGuizmo::OPERATION op = (m_gizmoOperation == 0 ? ImGuizmo::TRANSLATE :
                    (m_gizmoOperation == 1 ? ImGuizmo::ROTATE : ImGuizmo::SCALE));
                ImGuizmo::MODE mode = m_gizmoLocalSpace ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

                ImGuizmo::Manipulate(glm::value_ptr(currentViewMatrix),
                    glm::value_ptr(currentProjectionMatrix),
                    op, mode,
                    glm::value_ptr(modelMatrix));

                if (ImGuizmo::IsUsing())
                {
                    glm::vec3 newWorldPosition, newScale;
                    float rotationEuler[3];
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix),
                        glm::value_ptr(newWorldPosition),
                        rotationEuler,
                        glm::value_ptr(newScale));
                    glm::vec3 eulerRadians = glm::radians(glm::vec3(rotationEuler[0], rotationEuler[1], rotationEuler[2]));
                    glm::quat newWorldRotation = glm::quat(eulerRadians);

                    if (parentTransform)
                    {
                        glm::mat4 parentWorldMatrix = parentTransform->worldMatrix;

                        glm::mat4 inverseParentMatrix = glm::inverse(parentWorldMatrix);
                        glm::vec4 localPos4 = inverseParentMatrix * glm::vec4(newWorldPosition, 1.0f);
                        glm::vec3 newLocalPosition = glm::vec3(localPos4);

                        glm::vec3 parentScale = parentTransform->scale;
                        glm::vec3 newLocalScale = newScale;

                        entityTransform->position = newLocalPosition;
                        entityTransform->rotation = newWorldRotation;
                        entityTransform->scale = newLocalScale;
                    }
                    else if (newWorldPosition.length() > 0.5f)
                    {
                        entityTransform->position = newWorldPosition;
                        entityTransform->rotation = newWorldRotation;
                        entityTransform->scale = newScale;
                    }

                    entityTransform->markDirty();

                    HierarchyUtils::markChildrenDirty(m_world, m_selectedEntity);
                }
            }
        }

        if (m_cursorLockApplied)
        {
            drawCrosshair(imagePos, imageSize);
        }

        const bool clickedViewport = !m_isPlaying
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && ImGui::IsWindowHovered()
            && !ImGui::IsAnyItemHovered()
            && !ImGuizmo::IsOver()
            && !ImGuizmo::IsUsing();

        if (clickedViewport)
        {
            selectEntityAt(imagePos, imageSize);
        }

        if (!m_cursorLockApplied)
        {
            ImVec2 toolbarPos = ImGui::GetWindowPos();
            ImVec2 toolbarSize = ImGui::GetContentRegionAvail();
            renderToolbar(toolbarPos, toolbarSize);
        }
    }

    ImGui::End();
}

void NvrhiRenderAdapter::applyCursorLock()
{
    InputHandler& input = InputHandler::getInstance();

    if (input.isCursorLocked() && !m_isPlaying)
    {
        input.setCursorLocked(false);
    }

    const bool locked = input.isCursorLocked();
    if (locked == m_cursorLockApplied) return;
    m_cursorLockApplied = locked;

    glfwSetInputMode(m_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, locked ? GLFW_TRUE : GLFW_FALSE);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (locked)
    {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
    else
    {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    input.skipNextMouseDelta();
}

void NvrhiRenderAdapter::drawScreenFlash(const glm::vec3& color, float alpha, ImVec2 imagePos, ImVec2 imageSize)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto withAlpha = [&](float value)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, std::clamp(value, 0.0f, 1.0f)));
    };

    const ImU32 edge = withAlpha(alpha);
    const ImU32 clear = withAlpha(0.0f);
    const ImVec2 min = imagePos;
    const ImVec2 max(imagePos.x + imageSize.x, imagePos.y + imageSize.y);
    const float bandX = imageSize.x * 0.22f;
    const float bandY = imageSize.y * 0.28f;

    drawList->AddRectFilled(min, max, withAlpha(alpha * 0.25f));
    drawList->AddRectFilledMultiColor(min, ImVec2(max.x, min.y + bandY), edge, edge, clear, clear);
    drawList->AddRectFilledMultiColor(ImVec2(min.x, max.y - bandY), max, clear, clear, edge, edge);
    drawList->AddRectFilledMultiColor(min, ImVec2(min.x + bandX, max.y), edge, clear, clear, edge);
    drawList->AddRectFilledMultiColor(ImVec2(max.x - bandX, min.y), max, clear, edge, edge, clear);
}

void NvrhiRenderAdapter::drawCenterMessage(const std::string& message, ImVec2 imagePos, ImVec2 imageSize)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * 2.2f;

    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= message.size())
    {
        size_t end = message.find('\n', start);
        if (end == std::string::npos) end = message.size();
        lines.push_back(message.substr(start, end - start));
        start = end + 1;
    }

    float width = 0.0f;
    for (const std::string& line : lines)
    {
        width = std::max(width, font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, line.c_str()).x);
    }

    const float lineHeight = fontSize * 1.2f;
    const float height = lineHeight * lines.size();
    const ImVec2 center(imagePos.x + imageSize.x * 0.5f, imagePos.y + imageSize.y * 0.4f);
    const float padding = fontSize * 0.6f;

    drawList->AddRectFilled(
        ImVec2(center.x - width * 0.5f - padding, center.y - height * 0.5f - padding * 0.6f),
        ImVec2(center.x + width * 0.5f + padding, center.y + height * 0.5f + padding * 0.6f),
        IM_COL32(0, 0, 0, 170), 8.0f);

    float y = center.y - height * 0.5f;
    for (const std::string& line : lines)
    {
        const float lineWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, line.c_str()).x;
        drawList->AddText(font, fontSize, ImVec2(center.x - lineWidth * 0.5f, y), IM_COL32(255, 235, 200, 255), line.c_str());
        y += lineHeight;
    }
}

void NvrhiRenderAdapter::drawCrosshair(ImVec2 imagePos, ImVec2 imageSize)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 center(imagePos.x + imageSize.x * 0.5f, imagePos.y + imageSize.y * 0.5f);
    const float size = 7.0f;
    const float gap = 3.0f;

    for (int pass = 0; pass < 2; ++pass)
    {
        const ImU32 color = pass == 0 ? IM_COL32(0, 0, 0, 160) : IM_COL32(255, 255, 255, 230);
        const float thickness = pass == 0 ? 4.0f : 2.0f;

        drawList->AddLine(ImVec2(center.x - size - gap, center.y), ImVec2(center.x - gap, center.y), color, thickness);
        drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + size + gap, center.y), color, thickness);
        drawList->AddLine(ImVec2(center.x, center.y - size - gap), ImVec2(center.x, center.y - gap), color, thickness);
        drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + size + gap), color, thickness);
    }
}

void NvrhiRenderAdapter::selectEntityAt(ImVec2 imagePos, ImVec2 imageSize)
{
    if (!m_world || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;

    const ImVec2 mouse = ImGui::GetMousePos();
    const float u = (mouse.x - imagePos.x) / imageSize.x;
    const float v = (mouse.y - imagePos.y) / imageSize.y;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return;

    const glm::vec2 ndc(u * 2.0f - 1.0f, 1.0f - v * 2.0f);
    const Ray ray = RayCast::fromScreen(ndc, currentViewMatrix, currentProjectionMatrix);

    RayHit hit;
    m_selectedEntity = RayCast::pickMesh(*m_world, ray, hit) ? hit.entity : -1;
}

void NvrhiRenderAdapter::renderSceneHierarchy()
{
    ImGui::Begin("Scene Hierarchy");

    if (!m_world)
    {
        ImGui::End();
        return;
    }

    auto& transforms = m_world->getComponentPool<Transform>();
    auto& hierarchies = m_world->getComponentPool<Hierarchy>();

    std::vector<EntityId> rootEntities;

    const auto& transEnts = transforms.entities();
    for (size_t i = 0; i < transEnts.size(); ++i)
    {
        const EntityId entity = transEnts[i];

        Hierarchy* hierarchy = hierarchies.getComponent(entity);
        if (!hierarchy || hierarchy->parent == static_cast<EntityId>(-1))
        {
            rootEntities.push_back(entity);
        }
    }

    for (EntityId root : rootEntities)
    {
        renderHierarchyNode(hierarchies, transforms, root, 0);
    }

    ImGui::End();
}

void NvrhiRenderAdapter::renderInspector()
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

    ImGui::BeginDisabled(!canDeleteEntity(m_selectedEntity));
    if (ImGui::SmallButton("Delete Entity"))
    {
        requestDeleteEntity(m_selectedEntity);
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    if (Transform* transform = m_world->getComponent<Transform>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            renderTransformEditor(*transform);
        }
    }

    if (Tag* tag = m_world->getComponent<Tag>(m_selectedEntity))
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
                m_world->getComponentPool<Tag>().addComponent(m_selectedEntity, Tag{});
            }
        }
    }

    if (MeshRenderer* meshRenderer = m_world->getComponent<MeshRenderer>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Mesh Renderer"))
        {
            ImGui::Checkbox("Visible", &meshRenderer->visible);
        }
    }

    if (Rigidbody* rb = m_world->getComponent<Rigidbody>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Rigidbody"))
        {
            ImGui::Checkbox("Use Gravity", &rb->useGravity);
            ImGui::Checkbox("Is Kinematic", &rb->isKinematic);
            {
                rb->updateInvMass();
            }
            ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.01f, 1000.0f);
            ImGui::Text("Velocity: %.2f, %.2f, %.2f", rb->velocity.x, rb->velocity.y, rb->velocity.z);
        }
    }

    if (Collider* collider = m_world->getComponent<Collider>(m_selectedEntity))
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

    if (Light* light = m_world->getComponent<Light>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enabled", &light->enabled);

            const char* lightTypes[] = { "Directional", "Point", "Spot" };
            int currentType = static_cast<int>(light->type);
            if (ImGui::Combo("Type", &currentType, lightTypes, 3))
            {
                light->type = static_cast<LightType>(currentType);
            }

            ImGui::Separator();

            ImGui::ColorEdit3("Color", glm::value_ptr(light->color));
            ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 10.0f);

            ImGui::Separator();

            if (light->type == LightType::Directional)
            {
                ImGui::DragFloat3("Direction", glm::value_ptr(light->direction), 0.1f, -1.0f, 1.0f);

                if (ImGui::Button("Normalize Direction"))
                {
                    light->direction = glm::normalize(light->direction);
                }
            }
            else if (light->type == LightType::Point)
            {
                ImGui::DragFloat("Constant", &light->constant, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Linear", &light->linear, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Quadratic", &light->quadratic, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Range", &light->range, 1.0f, 0.0f, 100.0f);
            }
            else if (light->type == LightType::Spot)
            {
                ImGui::DragFloat("Constant", &light->constant, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Linear", &light->linear, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Quadratic", &light->quadratic, 0.01f, 0.0f, 10.0f);

                float cutOffDeg = glm::degrees(glm::acos(light->cutOff));
                float outerCutOffDeg = glm::degrees(glm::acos(light->outerCutOff));

                if (ImGui::DragFloat("Cut Off", &cutOffDeg, 0.5f, 0.0f, 90.0f))
                {
                    light->cutOff = glm::cos(glm::radians(cutOffDeg));
                }
                if (ImGui::DragFloat("Outer Cut Off", &outerCutOffDeg, 0.5f, 0.0f, 90.0f))
                {
                    light->outerCutOff = glm::cos(glm::radians(outerCutOffDeg));
                }

                ImGui::DragFloat3("Direction", glm::value_ptr(light->direction), 0.1f, -1.0f, 1.0f);

                if (ImGui::Button("Normalize Direction"))
                {
                    light->direction = glm::normalize(light->direction);
                }
            }

            ImGui::Separator();

            if (light->type != LightType::Directional)
            {
                if (m_world->getComponentPool<Transform>().hasComponent(m_selectedEntity))
                {
                    Transform* transform = m_world->getComponent<Transform>(m_selectedEntity);
                    if (transform)
                    {
                        ImGui::Text("World Position: (%.2f, %.2f, %.2f)",
                            transform->getWorldPosition().x,
                            transform->getWorldPosition().y,
                            transform->getWorldPosition().z);
                    }
                }
            }
        }
    }

    if (Camera* camera = m_world->getComponent<Camera>(m_selectedEntity))
    {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Checkbox("Active", &camera->isActive))
            {
                if (camera->isActive)
                {
                    auto& camPool = m_world->getComponentPool<Camera>();
                    auto& cams = camPool.components();
                    auto& camEnts = camPool.entities();

                    for (size_t i = 0; i < cams.size(); ++i)
                    {
                        if (camEnts[i] != m_selectedEntity)
                            cams[i].isActive = false;
                    }
                }
            }

            ImGui::Separator();

            ImGui::DragFloat("Field of View", &camera->fov, 0.5f, 1.0f, 120.0f);
            if (camera->isActive)
                ImGui::Text("Aspect Ratio (viewport): %.3f", getAspectRatio());
            else
                ImGui::DragFloat("Aspect Ratio", &camera->aspectRatio, 0.01f, 0.1f, 3.0f);
            ImGui::DragFloat("Near Plane", &camera->nearPlane, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Far Plane", &camera->farPlane, 1.0f, 10.0f, 1000.0f);

            ImGui::Separator();

            if (ImGui::Button("Reset to Default"))
            {
                camera->fov = 45.0f;
                camera->aspectRatio = getAspectRatio();
                camera->nearPlane = 0.1f;
                camera->farPlane = 100.0f;
            }
        }
    }

    if (ImGui::CollapsingHeader("+ Add Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Tag
        if (!m_world->getComponentPool<Tag>().hasComponent(m_selectedEntity))
        {
            if (ImGui::MenuItem("Add Tag"))
            {
                m_world->getComponentPool<Tag>().addComponent(m_selectedEntity, Tag{});
            }
        }

        // MeshRenderer
        if (!m_world->getComponentPool<MeshRenderer>().hasComponent(m_selectedEntity))
        {
            if (ImGui::MenuItem("Add Mesh Renderer"))
            {
                auto vertShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.vert");
                auto fragShader = RESOURCE_MANAGER.load<Shader>("Shaders/Default.frag");

                if (vertShader && fragShader)
                {
                    auto cubeMesh = MeshFactory::createCube();

                    auto defaultMaterial = std::make_shared<Material>(
                        glm::vec4(0.8f, 0.8f, 0.8f, 1.0f),
                        vertShader->get(),
                        fragShader->get()
                    );

                    m_world->getComponentPool<MeshRenderer>().addComponent(m_selectedEntity, MeshRenderer{ cubeMesh, defaultMaterial });
                }
            }
        }

        // Light
        if (!m_world->getComponentPool<Light>().hasComponent(m_selectedEntity))
        {
            if (ImGui::MenuItem("Add Light"))
            {
                m_world->getComponentPool<Light>().addComponent(m_selectedEntity, Light{});
            }
        }

        // Camera
        if (!m_world->getComponentPool<Camera>().hasComponent(m_selectedEntity))
        {
            if (ImGui::MenuItem("Add Camera"))
            {
                Camera cam;
                cam.aspectRatio = getAspectRatio();
                cam.fov = 45.0f;
                cam.nearPlane = 0.1f;
                cam.farPlane = 100.0f;

                cam.isActive = m_world->getComponentPool<Camera>().size() == 0;

                m_world->getComponentPool<Camera>().addComponent(m_selectedEntity, cam);
            }
        }

        // Rigidbody
        if (!m_world->getComponentPool<Rigidbody>().hasComponent(m_selectedEntity))
        {
            if (ImGui::MenuItem("Add Rigidbody"))
            {
                m_world->getComponentPool<Rigidbody>().addComponent(m_selectedEntity, Rigidbody{});
            }
        }

        // Collider
        if (!m_world->getComponentPool<Collider>().hasComponent(m_selectedEntity))
        {
            if (ImGui::MenuItem("Add Collider"))
            {
                m_world->getComponentPool<Collider>().addComponent(m_selectedEntity, Collider{});
            }
        }

        // Script
        if (!m_world->getComponentPool<Script>().hasComponent(m_selectedEntity))
        {
            ImGui::BeginDisabled(m_isPlaying);
            if (ImGui::BeginCombo("Add Script", "Select class..."))
            {
                const auto classes = ScriptEngine::getInstance().listBehaviourClasses();
                if (classes.empty())
                {
                    ImGui::TextDisabled("No behaviour classes in scripts/");
                }

                for (const auto& behaviourClass : classes)
                {
                    const std::string label = behaviourClass.moduleName + "." + behaviourClass.className;
                    if (ImGui::Selectable(label.c_str()))
                    {
                        m_world->getComponentPool<Script>().addComponent(m_selectedEntity,
                            Script(behaviourClass.moduleName, behaviourClass.className));
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
        }
    }

    if (Script* script = m_world->getComponent<Script>(m_selectedEntity))
    {
        ImGui::PushID("ScriptComponent");

        ScriptSystem* activeScripts = ScriptEngine::getInstance().getActiveSystem();
        const bool isPlaying = activeScripts != nullptr;

        // Prefab instance: compare with the template, apply or revert
        PrefabInstance* instance = m_world->getComponent<PrefabInstance>(m_selectedEntity);
        std::optional<Script> prefabScript;
        if (instance && m_prefabLibrary)
        {
            prefabScript = m_prefabLibrary->getScript(instance->prefab);
        }
        const bool linked = prefabScript &&
            prefabScript->moduleName == script->moduleName &&
            prefabScript->className == script->className;

        ScriptInspector::DrawOptions options;
        if (linked) options.baseline = &prefabScript->fields;

        const nlohmann::json previousFields = isPlaying ? script->fields : nlohmann::json();

        if (ScriptInspector::draw(*script, *m_world, options) && isPlaying)
        {
            activeScripts->applyFieldEdits(m_selectedEntity, *script, previousFields);
        }

        ImGui::BeginDisabled(isPlaying);

        if (instance)
        {
            ImGui::Separator();
            ImGui::Text("Prefab: %s", instance->prefab.c_str());

            if (!prefabScript)
            {
                ImGui::TextDisabled("Template is missing or has no script");
            }
            else if (!linked)
            {
                ImGui::TextDisabled("Template uses another behaviour class");
            }
            else
            {
                if (ImGui::Button("Apply fields to prefab"))
                {
                    nlohmann::json merged = prefabScript->fields.is_object()
                        ? prefabScript->fields : nlohmann::json::object();
                    merged.update(script->fields);
                    m_prefabLibrary->saveScriptFields(instance->prefab, merged);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Next spawns of '%s' use these values", instance->prefab.c_str());
                }

                ImGui::SameLine();

                if (ImGui::Button("Revert to prefab"))
                {
                    // Scene references are not part of the template, keep them
                    nlohmann::json reverted = prefabScript->fields.is_object()
                        ? prefabScript->fields : nlohmann::json::object();
                    for (const auto& item : script->fields.items())
                    {
                        if (EntityReferenceData::isReference(item.value()))
                        {
                            reverted[item.key()] = item.value();
                        }
                    }
                    script->fields = std::move(reverted);
                }
            }
        }

        ImGui::EndDisabled();

        if (isPlaying)
        {
            ImGui::TextWrapped("Field changes apply to the running object and are discarded on Stop.");
        }
        else if (ImGui::Button("Remove Script"))
        {
            m_world->getComponentPool<Script>().removeComponent(m_selectedEntity);
        }

        ImGui::PopID();
    }

    ImGui::End();
}

void NvrhiRenderAdapter::renderStatistics()
{
    ImGui::Begin("Statistics");

    static float fpsHistory[100] = { 0 };
    static int fpsIndex = 0;

    float deltaTime = m_world->getCurrentDeltaTime();

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
    ImGui::Text("Scene resolution: %.0f x %.0f", m_sceneImageSize.x, m_sceneImageSize.y);
    ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);

#ifdef NDEBUG
    ImGui::Text("Build: Release");
#else
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Build: Debug");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Unoptimized C++: scripting bindings and math run many times slower. Measure FPS in x64-release.");
    }
#endif

    if (m_world && ImGui::CollapsingHeader("Systems (ms per frame)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (const World::SystemTiming& timing : m_world->getSystemTimings())
        {
            ImGui::Text("%-18s %7.3f", timing.name.c_str(), timing.milliseconds);
        }
    }

    ImGui::Separator();
    if (m_world)
    {
        int totalEntities = 0;

        auto& transforms = m_world->getComponentPool<Transform>();
        totalEntities = static_cast<int>(transforms.size());

        ImGui::Text("Total Entities: %d", totalEntities);
    }

    ImGui::Separator();

    ImGui::Text("FPS History:");
    ImGui::PlotLines("##FPS", fpsHistory, 100, fpsIndex, nullptr, 0.0f, 120.0f, ImVec2(0, 60));

    ImGui::Separator();

    ImGui::End();
}

void NvrhiRenderAdapter::renderMainMenuBar()
{
    m_showSaveAsScenePanel = false;
    m_showLoadScenePanel = false;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save Scene"))
            {
                saveCurrentScene();
            }

            if (ImGui::MenuItem("Save Scene As..."))
            {
                m_showSaveAsScenePanel = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Load Scene"))
            {
                m_showLoadScenePanel = true;
            }

            ImGui::Separator();

            ImGui::MenuItem("Auto Save", nullptr, &m_autoSaveEnabled);

            ImGui::Separator();

            if (ImGui::MenuItem("Exit"))
            {
                if (m_autoSaveEnabled) saveCurrentScene();
                glfwSetWindowShouldClose(m_window, true);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Delete Selected", "Del", false, canDeleteEntity(m_selectedEntity)))
            {
                requestDeleteEntity(m_selectedEntity);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Empty Entity"))
            {
                if (m_onCreateEmptyCallback) m_onCreateEmptyCallback();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Cube"))
            {
                if (m_onCreateCubeCallback) m_onCreateCubeCallback();
            }

            if (ImGui::MenuItem("Sphere"))
            {
                if (m_onCreateSphereCallback) m_onCreateSphereCallback();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Statistics", nullptr, &m_showStatistics);
            ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
            ImGui::MenuItem("Log Console", nullptr, &m_showLogPanel);
            ImGui::MenuItem("Script Console", nullptr, &m_showScriptConsole);
            ImGui::MenuItem("Asset Browser", nullptr, &m_showAssetBrowser);
            ImGui::MenuItem("Prefabs", nullptr, &m_showPrefabs);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                m_showAbout = true;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (m_showLoadScenePanel)
    {
        ImGui::OpenPopup("LoadScenePanel");
    }
    renderLoadScenePanel();

    if (m_showSaveAsScenePanel)
    {
        ImGui::OpenPopup("SaveSceneAsPanel");
    }
    renderSaveSceneAsPanel();

    renderAboutWindow();
}

void NvrhiRenderAdapter::renderAboutWindow()
{
    if (!m_showAbout) return;

    ImGui::Begin("About", &m_showAbout, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Simple Game Engine");
    ImGui::Text("Version 1.0.0");
    ImGui::Separator();
    ImGui::Text("A custom game engine with ECS architecture");

    ImGui::End();
}

void NvrhiRenderAdapter::renderSaveSceneAsPanel()
{
    if (ImGui::BeginPopupModal("SaveSceneAsPanel", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char filename[256] = "scene.json";
        ImGui::InputText("Filename", filename, sizeof(filename));

        if (ImGui::Button("Save"))
        {
            m_currentScenePath = "assets\\scenes\\" + std::string(filename);
            saveCurrentScene();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NvrhiRenderAdapter::renderLoadScenePanel()
{
    if (ImGui::BeginPopupModal("LoadScenePanel", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char filename[256] = "scene.json";
        ImGui::InputText("Filename", filename, sizeof(filename));

        if (ImGui::Button("Load"))
        {
            m_currentScenePath = "assets\\scenes\\" + std::string(filename);
            loadLastScene();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NvrhiRenderAdapter::renderTransformEditor(Transform& transform)
{
    ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.1f);

    glm::vec3 rotation = glm::degrees(glm::eulerAngles(transform.rotation));
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 1.0f))
    {
        transform.rotation = glm::quat(glm::radians(rotation));
    }

    ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.1f, 0.01f);
}

void NvrhiRenderAdapter::renderToolbar(ImVec2 position, ImVec2 size)
{
    ImGui::SetNextWindowPos(ImVec2(position.x, position.y + TOOLBAR_OFFSET));
    ImGui::SetNextWindowSize(ImVec2(size.x, TOOLBAR_HEIGHT));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Toolbar", nullptr, flags))
    {
        ImGui::SameLine(10);
        if (!m_isPlaying)
        {
            if (ImGui::Button("Play"))
            {
                m_isPlaying = true;
                m_selectedEntity = -1;
                if (m_onPlayCallback) m_onPlayCallback();
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Stop"))
            {
                m_isPlaying = false;
                if (m_onStopCallback) m_onStopCallback();
            }
            ImGui::PopStyleColor();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 500);

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

        ImGui::SameLine();
        ImGui::Checkbox("Local", &m_gizmoLocalSpace);

        ImGui::SameLine(ImGui::GetWindowWidth() - 150);

        if (ImGui::Checkbox("Debug Colliders", &m_showDebugColliders))
        {
            if (m_onToggleDebugCallback)
            {
                m_onToggleDebugCallback(m_showDebugColliders);
            }
        }
    }
    ImGui::End();
}

void NvrhiRenderAdapter::renderLogPanel()
{
    ImGui::Begin("Log Console");

    ImGui::Checkbox("Auto-scroll", &m_autoScrollLogs);
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_logEntries.clear();
    }
    ImGui::SameLine();

    ImGui::Separator();

    ImGui::Text("Filters:");
    ImGui::SameLine();
    ImGui::Checkbox("Info", &m_showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &m_showWarning);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &m_showError);
    ImGui::SameLine();
    ImGui::Checkbox("ResourceManager", &m_showResourceManager);

    ImGui::Separator();

    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    std::unique_lock<std::mutex> logLock(m_logMutex);
    for (const auto& entry : m_logEntries)
    {
        bool shouldShow = false;
        switch (entry.level) 
        {
        case LogLevel::Info: shouldShow = m_showInfo; break;
        case LogLevel::Warning: shouldShow = m_showWarning; break;
        case LogLevel::Error: shouldShow = m_showError; break;
        case LogLevel::ResourceManager:
        case LogLevel::ResourceManagerError:
            shouldShow = m_showResourceManager;
            break;
        }

        if (!shouldShow) continue;

        ImVec4 color;
        switch (entry.level) 
        {
        case LogLevel::Info: color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
        case LogLevel::Warning: color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); break;
        case LogLevel::Error: color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
        case LogLevel::ResourceManager: color = ImVec4(0.4f, 0.8f, 0.4f, 1.0f); break;
        case LogLevel::ResourceManagerError: color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(entry.message.c_str());
        ImGui::PopStyleColor();
    }
    logLock.unlock();

    if (m_autoScrollLogs && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) 
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::End();
}

void NvrhiRenderAdapter::renderScriptConsole()
{
    ImGui::SetNextWindowSize(ImVec2(600.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (m_scriptConsoleFocusChange > 0)
    {
        ImGui::SetNextWindowCollapsed(false);
        ImGui::SetNextWindowFocus();
    }
    if (!ImGui::Begin("Script Console", &m_showScriptConsole))
    {
        m_scriptConsoleFocused = false;
        ImGui::End();
        return;
    }
    m_scriptConsoleFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (ImGui::Button("Clear"))
    {
        m_scriptConsole.clearOutput();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        m_scriptConsole.reset();
        m_scriptConsoleScrollToBottom = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Forget console variables and an unfinished block");
    }
    ImGui::SameLine();
    if (m_isPlaying)
    {
        ImGui::TextDisabled("F1: game/console | engine, sel = selected entity | Up/Down: history | Tab: indent");
    }
    else
    {
        ImGui::TextDisabled("Edit mode: the engine API (find, spawn, entities) works only in Play");
    }

    ImGui::Separator();

    const float footerHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScriptConsoleScrollRegion", ImVec2(0, -footerHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (const ScriptConsole::Line& line : m_scriptConsole.lines())
    {
        ImVec4 color;
        switch (line.type)
        {
        case ScriptConsole::LineType::Input: color = ImVec4(0.55f, 0.8f, 1.0f, 1.0f); break;
        case ScriptConsole::LineType::Output: color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f); break;
        case ScriptConsole::LineType::Error: color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f); break;
        case ScriptConsole::LineType::Info: color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }

    if (m_scriptConsoleScrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }
    m_scriptConsoleScrollToBottom = false;

    ImGui::EndChild();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(m_scriptConsole.needsMoreInput() ? "..." : ">>>");
    ImGui::SameLine();

    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_CallbackHistory
        | ImGuiInputTextFlags_CallbackCompletion;

    bool reclaimFocus = false;
    ImGui::PushItemWidth(-FLT_MIN);
    if (ImGui::InputText("##ScriptConsoleInput", m_scriptConsoleInput, sizeof(m_scriptConsoleInput),
        flags, &NvrhiRenderAdapter::scriptConsoleInputCallback, this))
    {
        m_scriptConsole.submit(m_scriptConsoleInput, m_selectedEntity);
        m_scriptConsoleInput[0] = '\0';
        m_scriptConsoleHistoryPos = -1;
        m_scriptConsoleScrollToBottom = true;
        reclaimFocus = true;
    }
    ImGui::PopItemWidth();

    if (reclaimFocus || m_scriptConsoleFocusChange > 0)
    {
        ImGui::SetKeyboardFocusHere(-1);
        m_scriptConsoleFocusChange = 0;
    }

    ImGui::End();
}

int NvrhiRenderAdapter::scriptConsoleInputCallback(ImGuiInputTextCallbackData* data)
{
    auto* self = static_cast<NvrhiRenderAdapter*>(data->UserData);

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        data->InsertChars(data->CursorPos, "    ");
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        const std::vector<std::string>& history = self->m_scriptConsole.history();
        const int previousPos = self->m_scriptConsoleHistoryPos;
        int& pos = self->m_scriptConsoleHistoryPos;

        if (data->EventKey == ImGuiKey_UpArrow && !history.empty())
        {
            if (pos == -1) pos = static_cast<int>(history.size()) - 1;
            else if (pos > 0) --pos;
        }
        else if (data->EventKey == ImGuiKey_DownArrow && pos != -1)
        {
            if (++pos >= static_cast<int>(history.size())) pos = -1;
        }

        if (pos != previousPos)
        {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, pos >= 0 ? history[pos].c_str() : "");
        }
    }

    return 0;
}

void NvrhiRenderAdapter::renderAssetBrowser()
{
    ZoneScopedN("AssetBrowser::render");

    if (!m_showAssetBrowser) return;

    ImGui::Begin("Asset Browser");

    if (ImGui::ArrowButton("##up", ImGuiDir_Up))
    {
        assetBrowserNavigateUp();
    }
    ImGui::SameLine();

    if (ImGui::Button("Refresh"))
    {
        AssetsUtils::scanAssetsDirectory(m_currentDirectory, m_currentDirectory, m_currentDirectoryContent);
    }

    ImGui::SameLine();
    ImGui::Text("Path: %s", m_currentDirectory.c_str());

    ImGui::Separator();

    std::string pathToShow = m_currentDirectory;
    size_t pos = 0;
    std::string accumulated = "";

    std::vector<std::string> pathParts;
    std::string temp = m_currentDirectory;
    while ((pos = temp.find("/")) != std::string::npos)
    {
        pathParts.push_back(temp.substr(0, pos));
        temp.erase(0, pos + 1);
    }
    if (!temp.empty()) pathParts.push_back(temp);

    accumulated = "";
    for (size_t i = 0; i < pathParts.size(); i++)
    {
        if (i > 0) accumulated += "/";
        accumulated += pathParts[i];

        if (ImGui::Button(pathParts[i].c_str()))
        {
            AssetsUtils::scanAssetsDirectory(accumulated, m_currentDirectory, m_currentDirectoryContent);
            break;
        }
        ImGui::SameLine();
        if (i < pathParts.size() - 1)
        {
            ImGui::Text("/");
            ImGui::SameLine();
        }
    }

    ImGui::Separator();

    float windowWidth = ImGui::GetContentRegionAvail().x;
    float itemWidth = 120.0f;
    int columns = (int)(windowWidth / itemWidth);
    if (columns < 1) columns = 1;

    ImGui::Columns(columns, nullptr, false);


    for (const auto asset : m_currentDirectoryContent)
    {
        ImGui::BeginGroup();

        if (asset.isDirectory)
        {
            if (m_folderIcon)
            {
                ImGui::Image(m_folderIcon, ImVec2(80, 80));
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[FOLDER]");
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                if (asset.name == "..")
                {
                    assetBrowserNavigateUp();
                }
                else
                {
                    std::string pathToEnter = asset.path;
                    AssetsUtils::scanAssetsDirectory(pathToEnter, m_currentDirectory, m_currentDirectoryContent);
                }
            }
        }
        else if (asset.extension == ".png" || asset.extension == ".jpg" || asset.extension == ".jpeg")
        {
            auto textureResource = AssetLoader::getInstance().requestTexture(asset.path);
            if (textureResource && textureResource->isReady() && textureResource->isValid())
            {
                Texture* tex = textureResource->get();
                unsigned int textureID = tex->m_handle;

                ImGui::Image((ImTextureID)(intptr_t)textureID, ImVec2(80, 80));

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Size: %dx%d\nPath: %s", tex->m_width, tex->m_height, asset.path.c_str());
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[TEX]");
            }
        }
        else if (asset.extension == ".obj" || asset.extension == ".fbx")
        {
            if (m_modelIcon)
                ImGui::Image(m_modelIcon, ImVec2(80, 80));
            else
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[MODEL]");
        }

        std::string displayName = asset.name;
        if (displayName.length() > 20) 
        {
            displayName = displayName.substr(0, 17) + "...";
        }
        ImGui::TextWrapped("%s", displayName.c_str());

        if (!asset.isDirectory)
        {
            std::string buttonLabel = "Use##" + asset.path;
            if (ImGui::Button(buttonLabel.c_str()))
            {
                m_currentAssetPath = asset.path;

                if (asset.extension == ".png" || asset.extension == ".jpg" || asset.extension == ".jpeg")
                {
                    auto textureResource = AssetLoader::getInstance().requestTexture(asset.path);
                    if (textureResource && textureResource->isValid())
                    {
                        m_selectedTexture = textureResource->get();
                    }
                }

                else if (asset.extension == ".obj" || asset.extension == ".fbx")
                {
                    auto meshResource = AssetLoader::getInstance().requestMesh(asset.path);
                    if (meshResource && meshResource->isValid())
                    {
                        // Aliasing pointer: keeps the cached Resource alive while the mesh is used
                        m_selectedMesh = std::shared_ptr<Mesh>(meshResource, meshResource->get());
                    }
                }
            }
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", asset.path.c_str());
        }

        ImGui::EndGroup();
        ImGui::NextColumn();
    }

    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("Selected Asset:");
    if (!m_currentAssetPath.empty())
    {
        ImGui::Text("%s", m_currentAssetPath.c_str());

        if (std::filesystem::path(m_currentAssetPath).extension().string() == ".obj"
            || std::filesystem::path(m_currentAssetPath).extension().string() == ".fbx")
        {
            ImGui::Text("Type: 3D Model");
            if (m_selectedMesh && !m_selectedMesh->ready)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Loading...");
            }
            else if (m_selectedMesh)
            {
                ImGui::Text("Vertices: %zu", m_selectedMesh->vertices.size());
                ImGui::Text("Indices: %zu", m_selectedMesh->indices.size());
            }

            if (m_selectedMesh && ImGui::Button("Create Object from Asset"))
            {
                if (m_onCreateFromAssetCallback)
                {
                    m_onCreateFromAssetCallback(m_selectedMesh, m_currentAssetPath);
                }
            }
        }
        else if (std::filesystem::path(m_currentAssetPath).extension().string() == ".png"
            || std::filesystem::path(m_currentAssetPath).extension().string() == ".jpg")
        {
            ImGui::Text("Type: Texture");
            if (m_selectedTexture && !m_selectedTexture->ready)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Loading...");
            }
            else if (m_selectedTexture)
            {
                ImGui::Text("Size: %dx%d", m_selectedTexture->m_width, m_selectedTexture->m_height);
            }

            if (m_selectedTexture && ImGui::Button("Apply to Selected Object"))
            {
                if (m_selectedEntity != -1 && m_world)
                {
                    if (MeshRenderer* renderer = m_world->getComponent<MeshRenderer>(m_selectedEntity))
                    {
                        if (renderer->material)
                        {
                            renderer->material->diffuseTexture = m_selectedTexture;
                            renderer->material->hasTexture = true;
                        }
                    }
                }
            }
        }
    }
    else
    {
        ImGui::Text("None selected");
    }

    ImGui::End();
}

void NvrhiRenderAdapter::SizeCallback(ImGuiSizeCallbackData* data)
{
    float aspectRatio = *(float*)data->UserData;
    float newHeight = data->DesiredSize.x / aspectRatio;
    data->DesiredSize.y = newHeight;
}

void NvrhiRenderAdapter::addLogEntry(LogLevel level, const std::string& message)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &t);
    timeStr[24] = '\0';

    std::string levelStr;
    switch (level) 
    {
    case LogLevel::Info: 
        levelStr = "INFO"; 
        break;
    case LogLevel::Warning: 
        levelStr = "WARNING"; 
        break;
    case LogLevel::Error: 
        levelStr = "ERROR"; 
        break;
    case LogLevel::ResourceManager: 
        levelStr = "RESOURCEMANAGER"; 
        break;
    case LogLevel::ResourceManagerError: 
        levelStr = "RESOURCEMANAGER ERROR"; 
        break;
    }

    std::string formatted = "[" + std::string(timeStr) + "] [" + levelStr + "] " + message;

    LogEntry entry;
    entry.message = formatted;
    entry.level = level;
    entry.timestamp = now;

    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logEntries.push_back(entry);

    while (m_logEntries.size() > m_maxLogEntries) 
    {
        m_logEntries.erase(m_logEntries.begin());
    }
}

void NvrhiRenderAdapter::renderHierarchyNode(ComponentPool<Hierarchy>& hierarchies, ComponentPool<Transform>& transforms, EntityId entity, int depth)
{
    std::string entityName = "Entity " + std::to_string(entity);
    if (m_world->getComponentPool<Tag>().hasComponent(entity))
    {
        Tag* tag = m_world->getComponent<Tag>(entity);
        if (tag && !tag->name.empty())
        {
            entityName = tag->name;
        }
    }

    std::string displayName = "";
    for (int i = 0; i < depth; i++)
    {
        displayName += "  ";
    }
    displayName += entityName;

    bool hasChildren = false;
    std::vector<EntityId> children;

    if (hierarchies.hasComponent(entity))
    {
        Hierarchy* hierarchy = hierarchies.getComponent(entity);
        children = hierarchy->children;
        hasChildren = !children.empty();
    }

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (!hasChildren)
    {
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (m_selectedEntity == entity)
    {
        nodeFlags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(entity);

    bool isOpen = false;

    if (hasChildren)
    {
        isOpen = ImGui::TreeNodeEx(displayName.c_str(), nodeFlags);
    }
    else
    {
        if (ImGui::Selectable(displayName.c_str(), m_selectedEntity == entity))
        {
            m_selectedEntity = entity;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ID: %d", entity);
        }
    }

    if (ImGui::IsItemClicked())
    {
        m_selectedEntity = entity;
    }

    if (ImGui::BeginPopupContextItem("EntityContextMenu"))
    {
        m_selectedEntity = entity;

        if (ImGui::MenuItem("Delete", "Del", false, canDeleteEntity(entity)))
        {
            requestDeleteEntity(entity);
        }
        if (hasChildren && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Children are deleted too");
        }

        ImGui::EndPopup();
    }

    if (hasChildren && isOpen)
    {
        for (EntityId child : children)
        {
            if (transforms.hasComponent(child))
            {
                renderHierarchyNode(hierarchies, transforms, child, depth + 1);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void NvrhiRenderAdapter::onFramebufferResize(int width, int height)
{
    m_width = std::max(1, width);
    m_height = std::max(1, height);
}

void NvrhiRenderAdapter::flushDebugDraw()
{
    gpu->drawLines(m_debugVertices, currentViewMatrix, currentProjectionMatrix);
}

void NvrhiRenderAdapter::saveCurrentScene()
{
    if (m_world && !m_currentScenePath.empty())
    {
        SceneSerializer::saveScene(*m_world, m_currentScenePath);
    }
    else if (m_world)
    {
        std::string defaultPath = "assets\\scenes\\last_scene.json";
        SceneSerializer::saveScene(*m_world, defaultPath);
    }
}

void NvrhiRenderAdapter::loadLastScene()
{
    if (std::filesystem::exists(m_currentScenePath))
    {
        SceneSerializer::loadScene(*this, *m_world, m_currentScenePath);
    }
}

void NvrhiRenderAdapter::assetBrowserNavigateUp()
{
    if (m_currentDirectory != "assets" && m_currentDirectory != ".")
    {
        std::filesystem::path parent = std::filesystem::path(m_currentDirectory).parent_path();
        AssetsUtils::scanAssetsDirectory(parent.string(), m_currentDirectory, m_currentDirectoryContent);
    }
}

bool NvrhiRenderAdapter::shouldClose() const
{
    return glfwWindowShouldClose(m_window);
}

void NvrhiRenderAdapter::clearEvents()
{
    keyEvents.clear();
    mouseButtonEvents.clear();
    mouseMoveEvents.clear();
    mouseScrollEvents.clear();


}

void NvrhiRenderAdapter::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (g_instance)
    {
        g_instance->onKey(key, action);
    }
}

void NvrhiRenderAdapter::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (g_instance)
    {
        g_instance->onMouseButton(button, action);
    }
}

void NvrhiRenderAdapter::cursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    if (g_instance)
    {
        g_instance->onMouseMove(xpos, ypos);
    }
}

void NvrhiRenderAdapter::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (g_instance)
    {
        g_instance->onMouseScroll(xoffset, yoffset);
    }
}

unsigned int NvrhiRenderAdapter::compileShaderSource(const std::string& source, ShaderType type)
{
    return gpu->compileShader(source, type);
}
