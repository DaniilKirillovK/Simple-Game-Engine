cmake_minimum_required(VERSION 3.24)
project ("GameEngine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLFW_DIR "${CMAKE_SOURCE_DIR}/libs/glfw")
set(GLM_DIR "${CMAKE_SOURCE_DIR}/libs/glm")
set(ASSIMP_DIR "${CMAKE_SOURCE_DIR}/libs/assimp")
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")
set(IMGUIZMO_DIR "${CMAKE_SOURCE_DIR}/libs/imgui/imguizmo")
set(PYBIND11_DIR "${CMAKE_SOURCE_DIR}/libs/pybind11")
set(PYTHON_DIR "${CMAKE_SOURCE_DIR}/libs/python")

include(cmake/Graphics.cmake)

file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUIZMO_DIR}/Imguizmo.cpp"
)

set(ASSIMP_LIB_DIR "${ASSIMP_DIR}/lib")
set(ASSIMP_LIB_NAME "assimp-vc143-mt")

add_executable(${PROJECT_NAME} ${IMGUI_SOURCES} ${IMGUIZMO_DIR} "GameEngine.cpp" "src/Application.cpp" "src/Logger.cpp" "src/GameplayState.cpp" "src/InputHandler.cpp" "src/NvrhiRenderAdapter.cpp" "src/GpuRenderer.cpp" "src/ViewportSwapChain.cpp" "src/Entity.cpp" "src/Component.cpp" "src/ComponentPool.cpp" "src/World.cpp" "src/Systems/RenderSystem.cpp" "src/Systems/CameraSystem.cpp" "src/Systems/MovementSystem.cpp" "src/Resources/Resource.cpp" "src/Resources/ResourceManager.cpp" "src/Resources/MeshLoader/MeshLoader.cpp" "src/Resources/ShaderLoader/ShaderLoader.cpp" "src/Resources/TextureLoader/TextureLoader.cpp" "src/Resources/TextureLoader/Texture.cpp" "src/Systems/PhysicsSystem.cpp" "src/Utils/HierarchyUtils.cpp" "src/Utils/json.hpp" "src/Systems/TransformSystem.cpp" "src/Utils/Serialization/SceneSerializer.cpp" "src/Utils/Assets/AssetsUtils.cpp" "external/tracy/TracyClient.cpp" "src/JobSystem/JobSystem.cpp" "src/Resources/AssetLoader.cpp" "src/Scripting/ScriptEngine.cpp" "src/Scripting/ScriptBindings.cpp" "src/Scripting/ScriptConsole.cpp" "src/Utils/RayCast.cpp" "src/Utils/EntityBounds.cpp" "src/Systems/TriggerTracker.cpp" "src/Systems/ScriptSystem.cpp" "src/Gameplay/PrefabLibrary.cpp" "src/Scripting/ScriptInspector.h" "src/Scripting/EntityReferenceData.h")

target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)
target_compile_definitions(${PROJECT_NAME} PRIVATE ENGINE_SCRIPTS_SOURCE_DIR="${CMAKE_SOURCE_DIR}/scripts")
target_compile_definitions(${PROJECT_NAME} PRIVATE ENGINE_PREFABS_SOURCE_DIR="${CMAKE_SOURCE_DIR}/assets/prefabs")
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
#add_compile_definitions(TRACY_ON_DEMAND)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${dxc_SOURCE_DIR}/inc"
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/tracy
    ${GLFW_DIR}/include
    ${GLM_DIR}
    ${ASSIMP_DIR}/include
    ${PYBIND11_DIR}/include
    ${PYTHON_DIR}/include
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${ASSIMP_LIB_DIR}
    ${PYTHON_DIR}/lib
)

target_link_libraries(${PROJECT_NAME}
    ${ASSIMP_LIB_NAME}
    python312
    "${dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib"
    engine_device
)


if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Refresh shader sources even when only HLSL changed.
add_custom_target(engine_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders" "$<TARGET_FILE_DIR:GameEngine>/Shaders")
add_dependencies(GameEngine engine_shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSIMP_DIR}/lib/${ASSIMP_LIB_NAME}.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/${ASSIMP_LIB_NAME}.dll
    COMMENT "Copying Assimp DLL to output directory"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PYTHON_DIR}/embed
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/python
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PYTHON_DIR}/embed/python312.dll
        ${PYTHON_DIR}/embed/python3.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts
    COMMENT "Copying Python runtime and scripts to output directory"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui.ini"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/imgui.ini"
    COMMENT "Copying imgui.ini to output directory"
)

add_compile_definitions(
    IMGUI_DISABLE_OBSOLETE_KEYIO=0
    IMGUI_HAS_VIEWPORT=1
)

configure_file(${CMAKE_SOURCE_DIR}/imgui.ini ${CMAKE_BINARY_DIR}/imgui.ini COPYONLY)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)
add_custom_command(TARGET GameEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
        "${dxc_SOURCE_DIR}/bin/x64/dxil.dll"
        $<TARGET_FILE_DIR:GameEngine>)
