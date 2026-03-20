#pragma once
#include <string>
#include <cstdint>

struct Color 
{
    uint8_t r, g, b, a;
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
};

class IRenderAdapter 
{
public:
    virtual ~IRenderAdapter() = default;
    virtual bool initialize(int width, int height) = 0;
    virtual void render() = 0;
    virtual void shutdown() = 0;
    virtual bool shouldClose() const = 0;
    virtual void compileShaders() = 0;
    virtual bool checkShaderCompileErrors(unsigned int shader, const std::string& type) = 0;

    static constexpr unsigned int DEFAULT_WIDTH = 800;
    static constexpr unsigned int DEFAULT_HEIGHT = 600;
};