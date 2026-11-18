#include "ViewportSwapChain.h"
#include <donut/app/DeviceManager_VK.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace rhi = nvrhi;
namespace
{
class WindowBuffers : public ViewportSwapChain
{
protected:
    GLFWwindow* window;
    rhi::IDevice* device;
    std::vector<rhi::FramebufferHandle> frames;
    int width = 0, height = 0;
    WindowBuffers(GLFWwindow* w, rhi::IDevice* d) : window(w), device(d) {}

    void addImage(rhi::ObjectType type, rhi::Object image, rhi::Format format)
    {
        auto desc = rhi::TextureDesc().setWidth(width).setHeight(height).setFormat(format)
            .setIsRenderTarget(true).enableAutomaticStateTracking(rhi::ResourceStates::Present);

        auto texture = device->createHandleForNativeTexture(type, image, desc);
        frames.push_back(device->createFramebuffer(rhi::FramebufferDesc().addColorAttachment(texture)));
    }

    void retire()
    {
        device->waitForIdle();
        device->runGarbageCollection();
        frames.clear();
    }
};

void check(HRESULT hr)
{
    if (FAILED(hr)) 
        throw std::runtime_error("Cannot present detached Direct3D 12 editor window: " + std::to_string(hr));
}

class DxWindow final : public WindowBuffers
{
    ComPtr<IDXGISwapChain3> chain;

public:
    DxWindow(GLFWwindow* w, rhi::IDevice* d) : WindowBuffers(w,d) {}
    ~DxWindow() override { retire(); }

    rhi::IFramebuffer* acquire() override
    {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (w <= 0 || h <= 0) return nullptr;
        if (frames.empty() || w != width || h != height)
        {
            retire();

            width = w;
            height = h;

            if (chain) 
            {
                check(chain->ResizeBuffers(2, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
            }
            else
            {
                DXGI_SWAP_CHAIN_DESC1 desc{};
                desc.Width = w;
                desc.Height = h;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                desc.BufferCount = 2;
                desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

                ComPtr<IDXGIFactory2> factory;
                check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

                ComPtr<IDXGISwapChain1> swap;
                ID3D12CommandQueue* queue = device->getNativeQueue(rhi::ObjectTypes::D3D12_CommandQueue, rhi::CommandQueue::Graphics);
                check(factory->CreateSwapChainForHwnd(queue, glfwGetWin32Window(window), &desc, nullptr, nullptr, &swap));
                check(swap.As(&chain));
            }

            for (UINT i = 0; i < 2; ++i)
            {
                ComPtr<ID3D12Resource> resource;
                check(chain->GetBuffer(i, IID_PPV_ARGS(&resource)));
                addImage(rhi::ObjectTypes::D3D12_Resource, resource.Get(), rhi::Format::RGBA8_UNORM);
            }
        }

        return frames.at(chain->GetCurrentBackBufferIndex());
    }

    void present() override { check(chain->Present(0, 0)); }
};

class VkWindow final : public WindowBuffers
{
    vk::Instance instance;
    vk::PhysicalDevice physical;
    vk::Device vkDevice;
    vk::Queue queue;
    vk::SurfaceKHR surface;
    vk::SwapchainKHR chain;
    vk::Fence acquired;
    std::vector<vk::Semaphore> finished;
    uint32_t index = 0;
    bool outdated = true;

    void releaseChain()
    {
        retire();

        for (auto semaphore : finished)
        {
            vkDevice.destroySemaphore(semaphore);
        }
        finished.clear();
        if (chain)
        {
            vkDevice.destroySwapchainKHR(chain);
        }

        chain = nullptr;
    }
    void resize(int w, int h)
    {
        releaseChain();

        auto caps = physical.getSurfaceCapabilitiesKHR(surface);
        width = int(caps.currentExtent.width == UINT32_MAX ? std::clamp(uint32_t(w), caps.minImageExtent.width, caps.maxImageExtent.width) : caps.currentExtent.width);
        height = int(caps.currentExtent.height == UINT32_MAX ? std::clamp(uint32_t(h), caps.minImageExtent.height, caps.maxImageExtent.height) : caps.currentExtent.height);

        auto formats = physical.getSurfaceFormatsKHR(surface);
        auto format = std::find_if(formats.begin(), formats.end(), [](auto f) {
            return f.format == vk::Format::eB8G8R8A8Unorm || f.format == vk::Format::eR8G8B8A8Unorm;
        });

        if (format == formats.end()) 
            throw std::runtime_error("No RGBA8 surface format for detached Vulkan window");

        uint32_t count = std::max(2u, caps.minImageCount);
        if (caps.maxImageCount)
        {
            count = std::min(count, caps.maxImageCount);
        }

        vk::SwapchainCreateInfoKHR info;
        info.setSurface(surface).setMinImageCount(count).setImageFormat(format->format).setImageColorSpace(format->colorSpace)
            .setImageExtent(vk::Extent2D(width,height)).setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
            .setImageSharingMode(vk::SharingMode::eExclusive).setPreTransform(caps.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque).setPresentMode(vk::PresentModeKHR::eFifo).setClipped(true);
        chain = vkDevice.createSwapchainKHR(info);
        for (auto image : vkDevice.getSwapchainImagesKHR(chain))
        {
            addImage(rhi::ObjectTypes::VK_Image, rhi::Object(VkImage(image)),
                format->format == vk::Format::eB8G8R8A8Unorm ? rhi::Format::BGRA8_UNORM : rhi::Format::RGBA8_UNORM);
            finished.push_back(vkDevice.createSemaphore({}));
        }

        outdated = false;
    }
public:
    VkWindow(GLFWwindow* w, rhi::IDevice* d) : WindowBuffers(w,d),
        instance(static_cast<VkInstance>(d->getNativeObject(rhi::ObjectTypes::VK_Instance))),
        physical(static_cast<VkPhysicalDevice>(d->getNativeObject(rhi::ObjectTypes::VK_PhysicalDevice))),
        vkDevice(static_cast<VkDevice>(d->getNativeObject(rhi::ObjectTypes::VK_Device))),
        queue(static_cast<VkQueue>(d->getNativeQueue(rhi::ObjectTypes::VK_Queue,rhi::CommandQueue::Graphics)))
    {
        VkSurfaceKHR nativeSurface{};
        if (glfwCreateWindowSurface(instance, w, nullptr, &nativeSurface) != VK_SUCCESS)
            throw std::runtime_error("Cannot create Vulkan surface for editor window");

        surface = nativeSurface;
        acquired = vkDevice.createFence({});
    }

    ~VkWindow() override
    {
        releaseChain();
        vkDevice.destroyFence(acquired);
        instance.destroySurfaceKHR(surface);
    }

    rhi::IFramebuffer* acquire() override
    {
        int w, h;
        glfwGetFramebufferSize(window,&w,&h);
        if (w <= 0 || h <= 0)
        {
            return nullptr;
        }
        if (outdated || w != width || h != height)
        {
            resize(w, h);
        }

        auto result = vkDevice.acquireNextImageKHR(chain, UINT64_MAX, {}, acquired, &index);
        if (result == vk::Result::eErrorOutOfDateKHR) 
        {
            outdated = true; 
            return nullptr;
        }
        outdated = result == vk::Result::eSuboptimalKHR;

        if (!outdated && result != vk::Result::eSuccess) 
            throw std::runtime_error("Vulkan viewport acquire failed");

        if (vkDevice.waitForFences(acquired,true,UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("Vulkan viewport acquire fence failed");

        vkDevice.resetFences(acquired);

        return frames.at(index);
    }
    void present() override
    {
        vk::SubmitInfo signal;
        signal.setSignalSemaphores(finished[index]);
        queue.submit(signal);

        vk::PresentInfoKHR info;
        info.setWaitSemaphores(finished[index]).setSwapchains(chain).setImageIndices(index);
        auto result = queue.presentKHR(&info);
        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
        {
            outdated = true;
        }
        else if (result != vk::Result::eSuccess) 
            throw std::runtime_error("Vulkan viewport presentation failed");
    }
};
}
std::unique_ptr<ViewportSwapChain> ViewportSwapChain::create(GLFWwindow* window, rhi::IDevice* device)
{
    if (device->getGraphicsAPI() == rhi::GraphicsAPI::VULKAN)
    {
        return std::make_unique<VkWindow>(window, device);
    }

    return std::make_unique<DxWindow>(window, device);
}
