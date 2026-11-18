#pragma once
#include <nvrhi/nvrhi.h>
#include <memory>
struct GLFWwindow;

class ViewportSwapChain
{
public:
    static std::unique_ptr<ViewportSwapChain> create(GLFWwindow* window, nvrhi::IDevice* device);

    virtual ~ViewportSwapChain() = default;
    virtual nvrhi::IFramebuffer* acquire() = 0;
    virtual void present() = 0;
};
