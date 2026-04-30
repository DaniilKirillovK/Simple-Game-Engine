cmake_minimum_required (VERSION 3.10)
project ("GameEngine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLFW_DIR "${CMAKE_SOURCE_DIR}/libs/glfw")
set(GLAD_DIR "${CMAKE_SOURCE_DIR}/glad")
set(GLM_DIR "${CMAKE_SOURCE_DIR}/libs/glm")
set(ASSIMP_DIR "${CMAKE_SOURCE_DIR}/libs/assimp")

list(APPEND SOURCES ${GLAD_DIR}/src/glad.c)

set(GLFW_LIB_DIR "${GLFW_DIR}/lib-vc2022")
set(GLFW_LIB_NAME "glfw3")

set(ASSIMP_LIB_DIR "${ASSIMP_DIR}/lib")
set(ASSIMP_LIB_NAME "assimp-vc143-mt")

add_executable(${PROJECT_NAME} "GameEngine.cpp" "src/Application.cpp" "src/Logger.cpp" "src/GameplayState.cpp" "src/InputHandler.cpp" "src/OpenGLRenderAdapter.cpp" ${GLAD_DIR}/src/glad.c "src/Entity.cpp" "src/Component.cpp" "src/ComponentPool.cpp" "src/World.cpp" "src/Systems/RenderSystem.cpp" "src/Systems/CameraSystem.cpp" "src/Systems/MovementSystem.cpp" "src/Resources/Resource.cpp" "src/Resources/ResourceManager.cpp" "src/Loaders/MeshLoader.cpp")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${GLAD_DIR}/include
    ${GLFW_DIR}/include
    ${GLM_DIR}
    ${ASSIMP_DIR}/include
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${GLFW_LIB_DIR}
    ${ASSIMP_LIB_DIR}
)

target_link_libraries(${PROJECT_NAME}
    ${GLFW_LIB_NAME}
    ${ASSIMP_LIB_NAME}
    opengl32
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} opengl32)
endif()

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Copy Assimp DLL to output directory after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSIMP_DIR}/lib/${ASSIMP_LIB_NAME}.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/${ASSIMP_LIB_NAME}.dll
    COMMENT "Copying Assimp DLL to output directory"
)

# Copy Assimp PDB (debug symbols) if it exists
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSIMP_DIR}/lib/${ASSIMP_LIB_NAME}.pdb"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/${ASSIMP_LIB_NAME}.pdb
    COMMENT "Copying Assimp PDB to output directory (if exists)"
)
