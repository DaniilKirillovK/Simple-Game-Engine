#include "GpuRenderer.h"
#include "ViewportSwapChain.h"
#include "NvrhiRenderAdapter.h"
#include "Components/MeshRenderer.h"
#include "Components/Light.h"
#include "Resources/AssetLoader.h"
#include <donut/app/DeviceManager.h>
#include <donut/app/DeviceManager_VK.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace rhi = nvrhi;
using donut::app::DeviceManager;
using Microsoft::WRL::ComPtr;

class VulkanWindowDevice final : public DeviceManager_VK
{
    void updateSurfaceExtent()
    {
        auto caps = m_VulkanPhysicalDevice.getSurfaceCapabilitiesKHR(m_WindowSurface);
        if (caps.currentExtent.width != UINT32_MAX)
        {
            m_DeviceParams.backBufferWidth = caps.currentExtent.width;
            m_DeviceParams.backBufferHeight = caps.currentExtent.height;
        }
    }

    bool CreateSwapChain() override
    {
        updateSurfaceExtent();
        return DeviceManager_VK::CreateSwapChain();
    }

    void ResizeSwapChain() override
    {
        updateSurfaceExtent();
        DeviceManager_VK::ResizeSwapChain();
    }
};

struct SceneInput 
{ 
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct OverlayInput 
{ 
    glm::vec3 position; 
    glm::vec4 color; 
    glm::vec2 uv; 
};

struct LightData 
{
    glm::vec4 colorIntensity;
    glm::vec4 positionType;
    glm::vec4 directionInner;
    glm::vec4 falloffOuter;
};

struct DrawConstants
{
    glm::mat4 model, viewProjection, normal;
    glm::vec4 diffuse, specularShininess, eye;
    uint32_t lightCount, textured, instanced, padding;
    std::array<LightData, 8> lights;
};

static_assert(sizeof(DrawConstants) == 768);

glm::mat4 clipTransform()
{
    glm::mat4 matrix(1.f);
    matrix[2][2] = 0.5f;
    matrix[3][2] = 0.5f;
    return matrix;
}

enum class Pass { Scene, Lines, Gui };
struct MeshBuffers { rhi::BufferHandle vertices, indices; };

std::string readShader(const char* path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error(std::string("Cannot open shader: ") + path);
    return {std::istreambuf_iterator<char>(file), {}};
}

// Compares two P6 PPM files written by the verification passes: true when the pixel
// payloads are identical, otherwise logs how many pixels differ and returns false.
bool comparePpmPayloads(const std::string& pathA, const std::string& pathB)
{
    auto load = [](const std::string& path, uint32_t& width, uint32_t& height, std::vector<uint8_t>& rgb) -> bool
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        std::string magic;
        int maxValue = 0;
        if (!(file >> magic >> width >> height >> maxValue) || magic != "P6" || maxValue != 255)
            return false;
        file.get(); // single whitespace character between the header and the payload

        rgb.resize(size_t(width) * height * 3);
        file.read(reinterpret_cast<char*>(rgb.data()), rgb.size());
        return file.gcount() == std::streamsize(rgb.size());
    };

    uint32_t widthA = 0, heightA = 0, widthB = 0, heightB = 0;
    std::vector<uint8_t> pixelsA, pixelsB;
    if (!load(pathA, widthA, heightA, pixelsA) || !load(pathB, widthB, heightB, pixelsB))
    {
        LOG_ERROR("Equivalence check: cannot read verification frames (" + pathA + ", " + pathB + ")");
        return false;
    }
    if (widthA != widthB || heightA != heightB || pixelsA.size() != pixelsB.size())
    {
        LOG_ERROR("Equivalence check: frame sizes differ (" + pathA + " " + std::to_string(widthA) + "x"
            + std::to_string(heightA) + " vs " + pathB + " " + std::to_string(widthB) + "x" + std::to_string(heightB) + ")");
        return false;
    }

    size_t differingPixels = 0;
    for (size_t i = 0; i + 2 < pixelsA.size(); i += 3)
    {
        if (pixelsA[i] != pixelsB[i] || pixelsA[i + 1] != pixelsB[i + 1] || pixelsA[i + 2] != pixelsB[i + 2])
            ++differingPixels;
    }

    if (differingPixels != 0)
    {
        LOG_ERROR("Equivalence check: " + std::to_string(differingPixels) + " of " + std::to_string(pixelsA.size() / 3)
            + " pixels differ between " + pathA + " and " + pathB);
    }
    return differingPixels == 0;
}

struct GpuRenderer::State
{
    bool vulkan;
    bool verify;
    int verifyPass = 0;
    unsigned frames = 0;
    bool errors = false;
    bool presentedEditorWindow = false;

    std::unique_ptr<DeviceManager> manager;

    rhi::IDevice* device = nullptr;
    rhi::CommandListHandle commands;
    rhi::BindingLayoutHandle layout;
    rhi::BufferHandle constants;
    rhi::SamplerHandle sampler;
    rhi::SamplerHandle wrapSampler;
    rhi::FramebufferHandle scene;
    rhi::TextureHandle color;
    rhi::TextureHandle depth;

    std::unordered_map<uint32_t, rhi::TextureHandle> textures;
    using BindingKey = std::tuple<uint32_t, bool, rhi::IBuffer*>;
    std::map<BindingKey, rhi::BindingSetHandle> bindings;
    std::unordered_map<uint32_t, rhi::ShaderHandle> shaders;

    rhi::IFramebuffer* guiTarget = nullptr;
    std::vector<ViewportSwapChain*> pendingWindows;
    std::map<std::tuple<Pass, uint32_t, uint32_t, rhi::Format>, rhi::GraphicsPipelineHandle> pipelines;

    std::unordered_map<const Mesh*, MeshBuffers> meshes;
    rhi::BufferHandle guiVertices;
    rhi::BufferHandle guiIndices;
    rhi::BufferHandle lineVertices;
    rhi::BufferHandle instanceIdentity; // 1-instance identity data, bound when a shader reads no instances
    std::vector<rhi::BufferHandle> instanceBuffers; // one structured buffer per batch in a frame
    size_t instanceBufferCursor = 0;                // resets every frame
    std::vector<rhi::BufferHandle> indirectArgBuffers; // one indirect-args buffer per batch in a frame
    size_t indirectArgsCursor = 0;                     // resets every frame

    RenderStats statsCounters;
    bool batchInfoLogged = false; // debug: one batch dump per verify pass

    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcIncludeHandler> includes;
    ShaderProgram overlay;
    uint32_t nextTexture = 2, nextShader = 1, white = 0;
    static constexpr uint32_t sceneId = 1;

    State(bool vk, bool verifyFrame) : vulkan(vk), verify(verifyFrame) {}

    uint32_t upload(const void* rgba, int width, int height, bool mipmaps = false)
    {
        auto desc = rhi::TextureDesc().setWidth(width).setHeight(height).setFormat(rhi::Format::RGBA8_UNORM)
            .enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource);

        if (mipmaps)
        {
            desc.mipLevels = 1u + uint32_t(std::log2(std::max(width, height)));
        }
        auto texture = device->createTexture(desc);
        if (!texture)
        {
            return 0;
        }

        auto uploadList = device->createCommandList(rhi::CommandListParameters().setEnableImmediateExecution(false));
        uploadList->open();
        uploadList->writeTexture(texture, 0, 0, rgba, size_t(width) * 4);

        const auto* source = static_cast<const uint8_t*>(rgba);
        std::vector<uint8_t> previous;
        for (uint32_t mip = 1; mip < desc.mipLevels; ++mip)
        {
            int w = std::max(1, width / 2);
            int h = std::max(1, height / 2);
            std::vector<uint8_t> next(size_t(w) * h * 4);
            for (int y = 0; y < h; ++y)
            {
                auto* top = source + size_t(y * 2) * width * 4;
                auto* bottom = source + size_t(std::min(height - 1, y * 2 + 1)) * width * 4;
                for (int x = 0; x < w; ++x)
                {
                    int left = x * 8;
                    int right = std::min(width - 1, x * 2 + 1) * 4;
                    for (int c = 0; c < 4; ++c)
                        next[(size_t(y) * w + x) * 4 + c] =
                            uint8_t((top[left+c] + top[right+c] + bottom[left+c] + bottom[right+c] + 2) / 4);
                }
            }
            uploadList->writeTexture(texture, 0, mip, next.data(), size_t(w) * 4);
            previous = std::move(next);
            source = previous.data();
            width = w;
            height = h;
        }
        uploadList->close();

        device->executeCommandList(uploadList);

        uint32_t id = nextTexture++;
        textures.emplace(id, texture);

        return id;
    }

    rhi::BufferHandle buffer(const void* data, size_t bytes, bool index)
    {
        auto desc = rhi::BufferDesc().setByteSize(bytes).setIsIndexBuffer(index).setIsVertexBuffer(!index)
            .enableAutomaticStateTracking(index ? rhi::ResourceStates::IndexBuffer : rhi::ResourceStates::VertexBuffer);
        auto result = device->createBuffer(desc);
        if (!result) 
            throw std::runtime_error("GPU buffer allocation failed");
        commands->writeBuffer(result, data, bytes);
        return result;
    }

    void stream(rhi::BufferHandle& storage, const void* data, size_t size, bool index)
    {
        if (!storage || storage->getDesc().byteSize < size)
        {
            auto desc = rhi::BufferDesc().setByteSize(size + size / 2).setIsIndexBuffer(index).setIsVertexBuffer(!index)
                .enableAutomaticStateTracking(index ? rhi::ResourceStates::IndexBuffer : rhi::ResourceStates::VertexBuffer);
            storage = device->createBuffer(desc);
            if (!storage) 
                throw std::runtime_error("Cannot allocate streaming geometry");
        }
        commands->writeBuffer(storage, data, size);
    }

    // Uploads per-instance transforms into a structured buffer (stride 64: one float4x4).
    // Each batch in a frame gets its own buffer from the pool: nvrhi's D3D12 backend must not
    // see two writeBuffer + draw cycles on the same buffer within one command list.
    rhi::IBuffer* instances(const std::vector<InstanceData>& data)
    {
        const size_t bytes = data.size() * sizeof(InstanceData);
        if (instanceBufferCursor >= instanceBuffers.size())
        {
            auto desc = rhi::BufferDesc().setByteSize(bytes).setStructStride(64)
                .enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource);
            auto buffer = device->createBuffer(desc);
            if (!buffer) 
                throw std::runtime_error("Cannot allocate instance buffer");
            instanceBuffers.push_back(buffer);
        }
        else if (instanceBuffers[instanceBufferCursor]->getDesc().byteSize < bytes)
        {
            auto desc = rhi::BufferDesc().setByteSize(bytes).setStructStride(64)
                .enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource);
            auto buffer = device->createBuffer(desc);
            if (!buffer) 
                throw std::runtime_error("Cannot allocate instance buffer");
            instanceBuffers[instanceBufferCursor] = buffer;
        }

        rhi::IBuffer* buffer = instanceBuffers[instanceBufferCursor++].Get();
        commands->writeBuffer(buffer, data.data(), bytes);
        return buffer;
    }

    // Uploads indirect draw arguments into a pooled indirect-arguments buffer.
    // Like the instance buffers, each batch in a frame gets its own buffer: nvrhi's
    // D3D12 backend must not see several writeBuffer + executeIndirect cycles on
    // one buffer within a single command list.
    rhi::IBuffer* indirect(const void* args, size_t bytes)
    {
        const size_t capacity = std::max<size_t>(bytes, 256);
        if (indirectArgsCursor >= indirectArgBuffers.size())
        {
            auto desc = rhi::BufferDesc().setByteSize(capacity).setIsDrawIndirectArgs(true)
                .enableAutomaticStateTracking(rhi::ResourceStates::IndirectArgument);
            auto buffer = device->createBuffer(desc);
            if (!buffer) 
                throw std::runtime_error("Cannot allocate indirect args buffer");
            indirectArgBuffers.push_back(buffer);
        }
        else if (indirectArgBuffers[indirectArgsCursor]->getDesc().byteSize < bytes)
        {
            auto desc = rhi::BufferDesc().setByteSize(capacity).setIsDrawIndirectArgs(true)
                .enableAutomaticStateTracking(rhi::ResourceStates::IndirectArgument);
            auto buffer = device->createBuffer(desc);
            if (!buffer) 
                throw std::runtime_error("Cannot allocate indirect args buffer");
            indirectArgBuffers[indirectArgsCursor] = buffer;
        }

        rhi::IBuffer* buffer = indirectArgBuffers[indirectArgsCursor++].Get();
        commands->writeBuffer(buffer, args, bytes);
        return buffer;
    }

    rhi::IBindingSet* binding(uint32_t texture, bool repeat, rhi::IBuffer* instanceData)
    {
        BindingKey key{ texture, repeat, instanceData };
        auto it = bindings.find(key);
        if (it != bindings.end()) return it->second;
        auto desc = rhi::BindingSetDesc()
            .addItem(rhi::BindingSetItem::ConstantBuffer(0, constants))
            .addItem(rhi::BindingSetItem::Texture_SRV(0, textures.at(texture)))
            .addItem(rhi::BindingSetItem::StructuredBuffer_SRV(1, instanceData))
            .addItem(rhi::BindingSetItem::Sampler(0, repeat ? wrapSampler : sampler));
        return bindings[key] = device->createBindingSet(desc, layout);
    }

    rhi::GraphicsState graphics(Pass pass, ShaderProgram program, uint32_t texture, rhi::IBuffer* instanceData)
    {
        auto* fb = pass == Pass::Gui ? guiTarget : scene.Get();
        auto& pipeline = pipelines[{pass, program.vertex, program.pixel, fb->getFramebufferInfo().colorFormats[0]}];
        if (!pipeline)
        {
            bool mesh = pass == Pass::Scene;
            uint32_t stride = mesh ? sizeof(SceneInput) : sizeof(OverlayInput);

            rhi::VertexAttributeDesc attrs[3];
            attrs[0].setName("POSITION").setFormat(rhi::Format::RGB32_FLOAT).setElementStride(stride);
            attrs[1].setName(mesh ? "NORMAL" : "COLOR").setFormat(mesh ? rhi::Format::RGB32_FLOAT : rhi::Format::RGBA32_FLOAT)
                .setOffset(12).setElementStride(stride);
            attrs[2].setName("TEXCOORD").setFormat(rhi::Format::RG32_FLOAT)
                .setOffset(mesh ? offsetof(SceneInput, uv) : offsetof(OverlayInput, uv)).setElementStride(stride);

            rhi::GraphicsPipelineDesc desc;
            desc.VS = shaders.at(program.vertex);
            desc.PS = shaders.at(program.pixel);
            desc.inputLayout = device->createInputLayout(attrs, 3, desc.VS);
            desc.bindingLayouts = {layout};
            desc.primType = pass == Pass::Lines ? rhi::PrimitiveType::LineList : rhi::PrimitiveType::TriangleList;
            desc.renderState.depthStencilState.setDepthTestEnable(pass != Pass::Gui)
                .setDepthWriteEnable(mesh).setDepthFunc(rhi::ComparisonFunc::LessOrEqual);
            desc.renderState.rasterState.setCullMode(mesh ? rhi::RasterCullMode::Back : rhi::RasterCullMode::None)
                .setFrontCounterClockwise(true).setScissorEnable(true);
            desc.renderState.blendState.targets[0].setBlendEnable(true)
                .setSrcBlend(rhi::BlendFactor::SrcAlpha).setDestBlend(rhi::BlendFactor::InvSrcAlpha)
                .setSrcBlendAlpha(rhi::BlendFactor::One).setDestBlendAlpha(rhi::BlendFactor::InvSrcAlpha);

            pipeline = device->createGraphicsPipeline(desc, fb->getFramebufferInfo());
            if (!pipeline) throw std::runtime_error("Graphics pipeline creation failed");
        }

        rhi::GraphicsState result{};
        result.framebuffer = fb;
        result.pipeline = pipeline;
        result.bindings = {binding(texture, pass == Pass::Scene, instanceData)};
        result.viewport.addViewportAndScissorRect(fb->getFramebufferInfo().getViewport());

        return result;
    }

    MeshBuffers& ensureMesh(const Mesh& mesh)
    {
        auto& buffers = meshes[&mesh];
        if (!buffers.vertices)
        {
            std::vector<SceneInput> vertices(mesh.vertices.size());
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                vertices[i] = { 
                    mesh.vertices[i],
                    i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0,1,0),
                    i < mesh.texCoords.size() ? mesh.texCoords[i] : glm::vec2(0) 
                };
            }

            buffers.vertices = buffer(vertices.data(), vertices.size()*sizeof(SceneInput), false);
            if (!mesh.indices.empty())
            {
                buffers.indices = buffer(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t), true);
            }

            mesh.buffersUploaded = true;
        }
        return buffers;
    }

    void fillConstants(DrawConstants& data, const Material& material, const std::vector<Light*>& lights,
        const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection,
        const glm::mat4& normal, bool instanced)
    {
        data.model = model;
        data.normal = normal;
        data.viewProjection = clipTransform() * projection * view;
        data.eye = glm::inverse(view)[3];
        data.diffuse = material.diffuseColor;
        data.specularShininess = glm::vec4(material.specularColor);
        data.specularShininess.w = material.shininess;
        data.lightCount = uint32_t(std::min(lights.size(), data.lights.size()));
        data.instanced = instanced ? 1 : 0;

        for (uint32_t i = 0; i < data.lightCount; ++i)
        {
            const Light& l = *lights[i];
            data.lights[i] = {
                glm::vec4(l.color,l.intensity), 
                glm::vec4(glm::vec3(l.position),int(l.type)),
                glm::vec4(l.direction,l.cutOff), 
                glm::vec4(l.constant,l.linear,l.quadratic,l.outerCutOff)
            };
        }
    }
};

GpuRenderer::GpuRenderer(bool vulkan, bool verify) : state(std::make_unique<State>(vulkan, verify)) {}
GpuRenderer::~GpuRenderer()
{
    auto manager = std::move(state->manager);
    if (state->device)
    {
        state->device->waitForIdle();
    }
    donut::log::ResetCallback();
    state.reset();
    if (manager)
    {
        manager->Shutdown();
    }
}

GLFWwindow* GpuRenderer::initialize(int width, int height)
{
    auto& s = *state;
    donut::log::EnableOutputToMessageBox(false);
    donut::log::SetCallback([&s](donut::log::Severity severity, const char* message) {
        if (severity >= donut::log::Severity::Error || std::strstr(message, "Validation Error:")) 
        { 
            s.errors = true; LOG_ERROR(message); 
        }
        else if (severity == donut::log::Severity::Warning) 
        { 
            LOG_WARNING(message); 
        }
        else 
        { 
            LOG_INFO(message); 
        }
    });

    if (s.vulkan)
    {
        s.manager = std::make_unique<VulkanWindowDevice>();
    }
    else
    {
        s.manager.reset(DeviceManager::Create(rhi::GraphicsAPI::D3D12));
    }

    donut::app::DeviceCreationParameters params;
    params.enablePerMonitorDPI = true;
    params.startMaximized = true;
    params.backBufferWidth = width;
    params.backBufferHeight = height;
    params.swapChainFormat = rhi::Format::RGBA8_UNORM;
    params.enableDebugRuntime = s.vulkan && s.verify && std::getenv("VK_LAYER_PATH");
    params.enableNvrhiValidationLayer = s.verify;
    params.vsyncEnabled = true;

    if (!s.manager) return nullptr;
    if (!s.vulkan)
    {
        if (!s.manager->CreateInstance(params)) return nullptr;

        std::vector<donut::app::AdapterInfo> adapters;
        if (!s.manager->EnumerateAdapters(adapters)) return nullptr;

        uint64_t bestMemory = 0;
        for (size_t i = 0; i < adapters.size(); ++i)
        {
            const auto& adapter = adapters[i];
            if (adapter.vendorID != 0x1414 && adapter.dedicatedVideoMemory > bestMemory)
            {
                bestMemory = adapter.dedicatedVideoMemory;
                params.adapterIndex = int(i);
            }
        }
    }
    if (!s.manager->CreateWindowDeviceAndSwapChain(params,
        s.vulkan ? "Simple Game Engine - Vulkan" : "Simple Game Engine - Direct3D 12"))
    {
        return nullptr;
    }

    s.device = s.manager->GetDevice();
    LOG_INFO(std::string(s.vulkan ? "Vulkan: " : "Direct3D 12: ") + s.manager->GetRendererString());

    if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&s.utils))) ||
        FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&s.compiler))) ||
        FAILED(s.utils->CreateDefaultIncludeHandler(&s.includes)))
    {
        return nullptr;
    }

    s.commands = s.device->createCommandList();
    s.layout = s.device->createBindingLayout(rhi::BindingLayoutDesc().setVisibility(rhi::ShaderType::All)
        .addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0))
        .addItem(rhi::BindingLayoutItem::Texture_SRV(0))
        .addItem(rhi::BindingLayoutItem::StructuredBuffer_SRV(1))
        .addItem(rhi::BindingLayoutItem::Sampler(0)));
    s.constants = s.device->createBuffer(rhi::BufferDesc().setByteSize(sizeof(DrawConstants))
        .setIsConstantBuffer(true).setIsVolatile(true).setMaxVersions(4096));
    s.sampler = s.device->createSampler(rhi::SamplerDesc().setAllFilters(true));
    s.wrapSampler = s.device->createSampler(rhi::SamplerDesc().setAllFilters(true).setAllAddressModes(rhi::SamplerAddressMode::Repeat));
    uint32_t white = 0xffffffff;
    s.white = s.upload(&white, 1, 1);

    // Identity instance data, bound for every non-instanced draw so that the
    // binding set always covers slot 1 of the binding layout.
    {
        s.instanceIdentity = s.device->createBuffer(rhi::BufferDesc().setByteSize(sizeof(InstanceData)).setStructStride(64)
            .enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource));
        if (!s.instanceIdentity)
        {
            return nullptr;
        }
        const float identity[2][16] = {
            { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
            { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 },
        };
        auto uploadList = s.device->createCommandList(rhi::CommandListParameters().setEnableImmediateExecution(false));
        uploadList->open();
        uploadList->writeBuffer(s.instanceIdentity, identity, sizeof(identity));
        uploadList->close();
        s.device->executeCommandList(uploadList);
    }
    s.overlay.vertex = compileShader(readShader("Shaders/Overlay.vert"), ShaderType::Vertex);
    s.overlay.pixel = compileShader(readShader("Shaders/Overlay.frag"), ShaderType::Fragment);

    if (!s.overlay.vertex || !s.overlay.pixel)
    {
        return nullptr;
    }

    glfwGetFramebufferSize(s.manager->GetWindow(), &width, &height);
    resizeScene(std::max(1, width), std::max(1, height));
    return s.manager->GetWindow();
}

uint32_t GpuRenderer::compileShader(const std::string& source, ShaderType type)
{
    auto& s = *state;
    bool vs = type == ShaderType::Vertex;
    std::vector<LPCWSTR> args = {L"-E", L"main", L"-T", vs ? L"vs_6_0" : L"ps_6_0", L"-I", L"Shaders"};
    if (s.vulkan)
    {
        args.insert(args.end(), { L"-spirv", L"-fspv-target-env=vulkan1.2",
        L"-fvk-use-dx-layout", L"-fvk-s-shift", L"128", L"0", L"-fvk-b-shift", L"256", L"0" });
    }

    DxcBuffer input{source.data(), source.size(), DXC_CP_UTF8};
    ComPtr<IDxcResult> result;
    if (FAILED(s.compiler->Compile(&input, args.data(), UINT32(args.size()), s.includes.Get(), IID_PPV_ARGS(&result))))
    {
        return 0;
    }

    HRESULT status;
    if (FAILED(result->GetStatus(&status)) || FAILED(status))
    {
        ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        LOG_ERROR(errors ? errors->GetStringPointer() : "DXC compilation failed");
        return 0;
    }

    ComPtr<IDxcBlob> blob;
    if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr)))
    {
        return 0;
    }

    auto shader = s.device->createShader(rhi::ShaderDesc().setShaderType(vs ? rhi::ShaderType::Vertex : rhi::ShaderType::Pixel),
        blob->GetBufferPointer(), blob->GetBufferSize());
    if (!shader)
    {
        return 0;
    }

    uint32_t id = s.nextShader++;
    s.shaders.emplace(id, shader);

    return id;
}

void GpuRenderer::resizeScene(int width, int height)
{
    auto& s = *state;
    if (width <= 0 || height <= 0) return;

    if (s.color && s.color->getDesc().width == unsigned(width) && s.color->getDesc().height == unsigned(height))
    {
        return;
    }

    auto desc = rhi::TextureDesc().setWidth(width).setHeight(height).setFormat(rhi::Format::RGBA8_UNORM)
        .setIsRenderTarget(true).enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource);
    s.color = s.device->createTexture(desc);
    s.depth = s.device->createTexture(desc.setFormat(rhi::Format::D32)
        .enableAutomaticStateTracking(rhi::ResourceStates::DepthWrite));
    s.scene = s.device->createFramebuffer(rhi::FramebufferDesc().addColorAttachment(s.color).setDepthAttachment(s.depth));
    for (auto it = s.bindings.begin(); it != s.bindings.end();)
    {
        if (std::get<0>(it->first) == State::sceneId) it = s.bindings.erase(it);
        else ++it;
    }
    s.textures[State::sceneId] = s.color;
}

uint32_t GpuRenderer::sceneTexture() const 
{ 
    return State::sceneId;
}

void GpuRenderer::run(const std::function<void()>& frame)
{
    auto& s = *state;
    s.manager->m_callbacks.beforeFrame = [](DeviceManager& manager, uint32_t) { manager.RenderNextFrameWhileUnfocused(); };

    s.manager->m_callbacks.beforeRender = [&](DeviceManager&, uint32_t) {
        s.commands->open();
        s.statsCounters = {};
        s.instanceBufferCursor = 0;
        s.indirectArgsCursor = 0;
        s.commands->clearTextureFloat(s.color, rhi::AllSubresources, rhi::Color(0.1f, 0.1f, 0.15f, 1));
        s.commands->clearDepthStencilTexture(s.depth, rhi::AllSubresources, true, 1.f, false, 0);

        auto rendered = s.color;
        frame();
        rhi::StagingTextureHandle readback;
        if (s.verify && ++s.frames >= 60 && AssetLoader::getInstance().pendingCount() == 0)
        {
            auto desc = rendered->getDesc();
            readback = s.device->createStagingTexture(desc, rhi::CpuAccessMode::Read);
            s.commands->copyTexture(readback, {}, rendered, {});
        }
        s.commands->close();
        s.device->executeCommandList(s.commands);

        for (auto* window : s.pendingWindows)
        {
            window->present();
            s.presentedEditorWindow = true;
        }
        s.pendingWindows.clear();

        if (readback)
        {
            s.device->waitForIdle();
            size_t pitch = 0;
            auto* pixels = static_cast<const unsigned char*>(s.device->mapStagingTexture(readback, {}, rhi::CpuAccessMode::Read, &pitch));
            if (!pixels) 
                throw std::runtime_error("Scene readback failed");

            auto& desc = rendered->getDesc();
            std::string file = s.vulkan ? "scene-vulkan" : "scene-d3d12";
            if (s.verifyPass == 1) file += "-off";
            file += ".ppm";
            std::ofstream image(file, std::ios::binary);
            image << "P6\n" << desc.width << ' ' << desc.height << "\n255\n";

            size_t visible = 0;
            std::vector<char> rgb(desc.width * 3);
            for (uint32_t y = 0; y < desc.height; ++y)
            {
                auto* row = pixels + pitch * y;
                for (uint32_t x = 0; x < desc.width; ++x)
                {
                    auto* p = row + x * 4;
                    visible += p[0] != pixels[0] || p[1] != pixels[1] || p[2] != pixels[2];
                    std::memcpy(&rgb[x * 3], p, 3);
                }
                image.write(rgb.data(), rgb.size());
            }

            s.device->unmapStagingTexture(readback);
            if (!image || visible < 100 || s.meshes.empty() || s.errors)
                throw std::runtime_error("Scene verification failed (see engine.log)");

            LOG_INFO("Verified GPU scene (pass " + std::to_string(s.verifyPass) + ") with " + std::to_string(visible) + " varying pixels");
            LOG_INFO("Draw stats: regular=" + std::to_string(s.statsCounters.regularDraws)
                + " instanced=" + std::to_string(s.statsCounters.instancedDraws)
                + " indirect=" + std::to_string(s.statsCounters.indirectDraws)
                + " batches=" + std::to_string(s.statsCounters.batches)
                + " instances=" + std::to_string(s.statsCounters.batchedInstances));
            if (s.presentedEditorWindow)
            {
                LOG_INFO("Verified detached editor window presentation");
            }

            glfwSetWindowShouldClose(s.manager->GetWindow(), GLFW_TRUE);
        }
    };

    s.manager->RunMessageLoop();
    s.manager->m_callbacks.beforeRender = {};

    if (s.verify && (s.frames < 60 || s.errors)) 
        throw std::runtime_error("Renderer verification interrupted");
}

uint32_t GpuRenderer::uploadTexture(const TextureData& data)
{
    if (!data.pixels || data.width <= 0 || data.height <= 0 || data.channels < 1 || data.channels > 4)
    {
        return 0;
    }

    if (data.channels == 4)
    {
        return state->upload(data.pixels, data.width, data.height, true);
    }

    std::vector<unsigned char> rgba(size_t(data.width) * data.height * 4);
    for (size_t i = 0; i < rgba.size()/4; ++i)
    {
        const auto* pixel = &data.pixels[i * data.channels];
        for (int c = 0; c < 3; ++c) rgba[i*4+c] = c < data.channels ? pixel[c] : 0;
        rgba[i*4+3] = 255;
    }

    return state->upload(rgba.data(), data.width, data.height, true);
}
void GpuRenderer::destroyTexture(uint32_t id)
{
    for (auto it = state->bindings.begin(); it != state->bindings.end();)
    {
        if (std::get<0>(it->first) == id) it = state->bindings.erase(it);
        else ++it;
    }
    state->textures.erase(id); 

}
void GpuRenderer::releaseMesh(const Mesh* mesh) 
{ 
    state->meshes.erase(mesh); 
}

void GpuRenderer::drawMesh(const Mesh& mesh, const ShaderProgram& program, const Material& material,
    const std::vector<Light*>& lights, const glm::mat4& model, const glm::mat4& view,
    const glm::mat4& projection, const glm::mat4& normal)
{
    if (mesh.vertices.empty() || !program.vertex || !program.pixel) return;

    auto& s = *state;
    auto& buffers = s.ensureMesh(mesh);

    DrawConstants data{};
    s.fillConstants(data, material, lights, model, view, projection, normal, false);

    uint32_t texture = material.diffuseTexture ? material.diffuseTexture->m_handle : 0;
    data.textured = material.hasTexture && s.textures.count(texture);
    s.commands->writeBuffer(s.constants, &data, sizeof(data));

    auto graphics = s.graphics(Pass::Scene, program, data.textured ? texture : s.white, s.instanceIdentity);
    graphics.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(buffers.vertices));

    if (buffers.indices)
    {
        graphics.indexBuffer.setOffset(0).setBuffer(buffers.indices).setFormat(rhi::Format::R32_UINT);
    }
    s.commands->setGraphicsState(graphics);

    if (buffers.indices)
    {
        s.commands->drawIndexed(rhi::DrawArguments().setVertexCount(uint32_t(mesh.indices.size())));
    }
    else s.commands->draw(rhi::DrawArguments().setVertexCount(uint32_t(mesh.vertices.size())));

    s.statsCounters.regularDraws++;
}

void GpuRenderer::drawMeshInstanced(const Mesh& mesh, const ShaderProgram& program, const Material& material,
    const std::vector<Light*>& lights, const glm::mat4& view, const glm::mat4& projection,
    const std::vector<InstanceData>& instances, bool indirect)
{
    if (mesh.vertices.empty() || !program.vertex || !program.pixel || instances.empty()) return;

    auto& s = *state;
    auto& buffers = s.ensureMesh(mesh);

    DrawConstants data{};
    s.fillConstants(data, material, lights, glm::mat4(1.0f), view, projection, glm::mat4(1.0f), true);

    uint32_t texture = material.diffuseTexture ? material.diffuseTexture->m_handle : 0;
    data.textured = material.hasTexture && s.textures.count(texture);
    s.commands->writeBuffer(s.constants, &data, sizeof(data));

    if (s.verify && !s.batchInfoLogged)
    {
        s.batchInfoLogged = true;
        LOG_INFO("instanced batch: instances=" + std::to_string(instances.size())
            + " texture=" + std::to_string(texture)
            + " textured=" + std::to_string(data.textured)
            + " indices=" + std::to_string(buffers.indices ? mesh.indices.size() : 0)
            + " vertices=" + std::to_string(mesh.vertices.size()));
    }

    rhi::IBuffer* instanceData = s.instances(instances);
    auto graphics = s.graphics(Pass::Scene, program, data.textured ? texture : s.white, instanceData);
    graphics.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(buffers.vertices));

    if (buffers.indices)
    {
        graphics.indexBuffer.setOffset(0).setBuffer(buffers.indices).setFormat(rhi::Format::R32_UINT);
    }

    if (indirect)
    {
        if (buffers.indices)
        {
            nvrhi::DrawIndexedIndirectArguments args{};
            args.indexCount = uint32_t(mesh.indices.size());
            args.instanceCount = uint32_t(instances.size());
            graphics.setIndirectParams(s.indirect(&args, sizeof(args)));
            s.commands->setGraphicsState(graphics);
            s.commands->drawIndexedIndirect(0, 1);
        }
        else
        {
            nvrhi::DrawIndirectArguments args{};
            args.vertexCount = uint32_t(mesh.vertices.size());
            args.instanceCount = uint32_t(instances.size());
            graphics.setIndirectParams(s.indirect(&args, sizeof(args)));
            s.commands->setGraphicsState(graphics);
            s.commands->drawIndirect(0, 1);
        }
        s.statsCounters.indirectDraws++;
    }
    else
    {
        s.commands->setGraphicsState(graphics);
        if (buffers.indices)
        {
            s.commands->drawIndexed(rhi::DrawArguments().setVertexCount(uint32_t(mesh.indices.size()))
                .setInstanceCount(uint32_t(instances.size())));
        }
        else
        {
            s.commands->draw(rhi::DrawArguments().setVertexCount(uint32_t(mesh.vertices.size()))
                .setInstanceCount(uint32_t(instances.size())));
        }
        s.statsCounters.instancedDraws++;
    }

    s.statsCounters.batchedInstances += uint32_t(instances.size());
    s.statsCounters.batches++;
}

RenderStats GpuRenderer::stats() const
{
    return state->statsCounters;
}

void GpuRenderer::setVerifyPass(int pass)
{
    state->verifyPass = pass;
    state->frames = 0;
    state->errors = false;
    state->batchInfoLogged = false;
}

bool GpuRenderer::compareVerifyFrames() const
{
    auto& s = *state;
    const std::string api = s.vulkan ? "scene-vulkan" : "scene-d3d12";
    return comparePpmPayloads(api + ".ppm", api + "-off.ppm");
}

void GpuRenderer::drawLines(const std::vector<DebugVertex>& lines, const glm::mat4& view, const glm::mat4& projection)
{
    if (lines.empty()) return;

    auto& s = *state;
    std::vector<OverlayInput> vertices;
    vertices.reserve(lines.size());

    for (auto& line : lines)
    {
        vertices.push_back({ line.position, line.color, {} });
    }

    s.stream(s.lineVertices, vertices.data(), vertices.size()*sizeof(OverlayInput), false);
    auto matrix = clipTransform() * projection * view;
    s.commands->writeBuffer(s.constants, &matrix, sizeof(matrix));
    auto graphics = s.graphics(Pass::Lines, s.overlay, s.white, s.instanceIdentity);
    graphics.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(s.lineVertices));
    s.commands->setGraphicsState(graphics);
    s.commands->draw(rhi::DrawArguments().setVertexCount(uint32_t(vertices.size())));
}

void GpuRenderer::initGui()
{
    auto& io = ImGui::GetIO();
    if (state->verify)
    {
        io.ConfigViewportsNoAutoMerge = true;
        ImGui::LoadIniSettingsFromDisk("imgui.ini");
        io.IniFilename = nullptr;
    }

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(uintptr_t(state->upload(pixels,width,height))));
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    io.BackendRendererUserData = this;

    auto& platform = ImGui::GetPlatformIO();
    platform.Renderer_CreateWindow = [](ImGuiViewport* vp) {
        auto* renderer = static_cast<GpuRenderer*>(ImGui::GetIO().BackendRendererUserData);
        vp->RendererUserData = ViewportSwapChain::create(static_cast<GLFWwindow*>(vp->PlatformHandle), renderer->state->device).release();
    };
    platform.Renderer_DestroyWindow = [](ImGuiViewport* vp) {
        delete static_cast<ViewportSwapChain*>(vp->RendererUserData);
        vp->RendererUserData = nullptr;
    };
    platform.Renderer_RenderWindow = [](ImGuiViewport* vp, void*) {
        auto* renderer = static_cast<GpuRenderer*>(ImGui::GetIO().BackendRendererUserData);
        auto* window = static_cast<ViewportSwapChain*>(vp->RendererUserData);
        auto& s = *renderer->state;

        if (auto* target = window->acquire())
        {
            s.guiTarget = target;
            renderer->drawGui(*vp->DrawData);
            s.pendingWindows.push_back(window);
            s.guiTarget = nullptr;
        }
    };

    io.BackendRendererName = "engine_nvrhi";
}

void GpuRenderer::drawGui(const ImDrawData& data)
{
    auto& s = *state;
    auto* target = s.guiTarget ? s.guiTarget : s.manager->GetCurrentFramebuffer(false);
    s.commands->clearTextureFloat(target->getDesc().colorAttachments[0].texture, rhi::AllSubresources, rhi::Color(0.05f));
    if (data.DisplaySize.x <= 0 || data.DisplaySize.y <= 0)
    {
        return;
    }

    s.guiTarget = target;
    glm::mat4 matrix(1.f);
    matrix[0][0] = 2.f/data.DisplaySize.x;
    matrix[1][1] = -2.f/data.DisplaySize.y;
    matrix[3][0] = -1.f - data.DisplayPos.x * matrix[0][0];
    matrix[3][1] = 1.f - data.DisplayPos.y * matrix[1][1];
    s.commands->writeBuffer(s.constants, &matrix, sizeof(matrix));

    std::vector<OverlayInput> vertices;
    std::vector<ImDrawIdx> indices;
    vertices.reserve(data.TotalVtxCount);
    indices.reserve(data.TotalIdxCount);
    for (int i = 0; i < data.CmdListsCount; ++i)
    {
        auto& list = *data.CmdLists[i];
        if (list.VtxBuffer.empty() || list.IdxBuffer.empty()) continue;

        indices.insert(indices.end(), list.IdxBuffer.begin(), list.IdxBuffer.end());
        for (const auto& v : list.VtxBuffer)
        {
            auto color = ImGui::ColorConvertU32ToFloat4(v.col);
            vertices.push_back({{v.pos.x,v.pos.y,0},{color.x,color.y,color.z,color.w},{v.uv.x,v.uv.y}});
        }
    }

    if (indices.empty()) 
    {
        s.guiTarget = nullptr; 
        return; 
    }

    s.stream(s.guiVertices, vertices.data(), vertices.size()*sizeof(OverlayInput), false);
    s.stream(s.guiIndices, indices.data(), indices.size()*sizeof(ImDrawIdx), true);

    uint32_t baseVertex = 0, baseIndex = 0;
    for (int i = 0; i < data.CmdListsCount; ++i)
    {
        auto& list = *data.CmdLists[i];
        if (list.VtxBuffer.empty() || list.IdxBuffer.empty()) continue;

        for (const auto& cmd : list.CmdBuffer)
        {
            if (cmd.UserCallback)
            {
                if (cmd.UserCallback != ImDrawCallback_ResetRenderState) cmd.UserCallback(&list, &cmd);
                continue;
            }
            auto tex = uint32_t(uintptr_t(cmd.TextureId));
            if (!s.textures.count(tex)) continue;

            auto graphics = s.graphics(Pass::Gui, s.overlay, tex, s.instanceIdentity);

            auto& rect = graphics.viewport.scissorRects[0];
            rect.minX = std::max(0, int((cmd.ClipRect.x - data.DisplayPos.x)*data.FramebufferScale.x));
            rect.minY = std::max(0, int((cmd.ClipRect.y - data.DisplayPos.y)*data.FramebufferScale.y));
            rect.maxX = std::min(rect.maxX, int((cmd.ClipRect.z - data.DisplayPos.x)*data.FramebufferScale.x));
            rect.maxY = std::min(rect.maxY, int((cmd.ClipRect.w - data.DisplayPos.y)*data.FramebufferScale.y));
            if (rect.width() <= 0 || rect.height() <= 0) continue;

            graphics.addVertexBuffer(rhi::VertexBufferBinding().setBuffer(s.guiVertices));
            graphics.indexBuffer.setOffset(0).setBuffer(s.guiIndices).setFormat(sizeof(ImDrawIdx)==2 ? rhi::Format::R16_UINT : rhi::Format::R32_UINT);

            s.commands->setGraphicsState(graphics);
            s.commands->drawIndexed(rhi::DrawArguments().setVertexCount(cmd.ElemCount)
                .setStartVertexLocation(baseVertex + cmd.VtxOffset).setStartIndexLocation(baseIndex + cmd.IdxOffset));
        }

        baseVertex += list.VtxBuffer.Size;
        baseIndex += list.IdxBuffer.Size;
    }

    s.guiTarget = nullptr;
}
