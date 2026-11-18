include(FetchContent)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "GameEngine requires a 64-bit Windows target")
endif()

FetchContent_Declare(donut
    GIT_REPOSITORY https://github.com/NVIDIA-RTX/Donut.git
    GIT_TAG 52efbe37d7f550ea1c5c7ee02b75404ec7c72c09
    GIT_SUBMODULES nvrhi
    SOURCE_SUBDIR unused)
FetchContent_MakeAvailable(donut)
set(NVRHI_WITH_DX11 OFF CACHE BOOL "")
set(NVRHI_INSTALL OFF CACHE BOOL "")

# Fetch release snapshots instead of the full generated Vulkan header history.
FetchContent_Declare(vulkan_headers URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.352.tar.gz
    URL_HASH SHA256=4850909d22a8a9767c27daea2b972e49d7c298560573d5b6221ee50db9bf49db)
FetchContent_Declare(directx_headers URL https://github.com/microsoft/DirectX-Headers/archive/refs/tags/v1.717.0-preview.tar.gz
    URL_HASH SHA256=8df9e5543bdf21456ddc9afe55cc576cf903c286e7bdb78390d703467f76d61b)
add_subdirectory("${donut_SOURCE_DIR}/nvrhi" "${CMAKE_BINARY_DIR}/nvrhi" EXCLUDE_FROM_ALL)
FetchContent_Declare(dxc
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip
    URL_HASH SHA256=9ad895a6b039e3a8f8c22a1009f866800b840a74b50db9218d13319e215ea8a4
    SOURCE_SUBDIR unused)
FetchContent_MakeAvailable(dxc)
add_library(engine_device STATIC
    "${donut_SOURCE_DIR}/src/app/DeviceManager.cpp"
    "${donut_SOURCE_DIR}/src/app/dx12/DeviceManager_DX12.cpp"
    "${donut_SOURCE_DIR}/src/app/vulkan/DeviceManager_VK.cpp"
    "${donut_SOURCE_DIR}/src/core/log.cpp")
target_compile_definitions(engine_device PUBLIC DONUT_WITH_DX12=1 DONUT_WITH_VULKAN=1 NOMINMAX)
target_include_directories(engine_device PUBLIC "${donut_SOURCE_DIR}/include" "${GLFW_DIR}/include")
target_link_libraries(engine_device PUBLIC nvrhi_d3d12 nvrhi_vk nvrhi Vulkan-Headers d3d12 dxgi dxguid shcore "${GLFW_DIR}/lib-vc2022/glfw3.lib")
