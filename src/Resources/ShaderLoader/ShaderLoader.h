//
// Created by semen on 14.04.2026.
//

#ifndef GAMEENGINE_SHADERLOADER_H
#define GAMEENGINE_SHADERLOADER_H
#include <memory>
#include <string>


class IRenderAdapter;
struct ShaderProgram;

class ShaderLoader
{
public:
    static std::shared_ptr<ShaderProgram> Load(
        const std::string& vertexPath,
        const std::string& fragmentPath,
        IRenderAdapter* renderAdapter
    );

private:
    static std::string ReadFile(const std::string& path);
};


#endif //GAMEENGINE_SHADERLOADER_H