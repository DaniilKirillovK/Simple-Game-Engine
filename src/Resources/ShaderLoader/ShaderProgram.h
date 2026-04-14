//
// Created by semen on 14.04.2026.
//

#ifndef GAMEENGINE_SHADERPROGRAM_H
#define GAMEENGINE_SHADERPROGRAM_H
#include <string>

struct ShaderProgram
{
    uint32_t programId = 0;

    std::string vertexPath;
    std::string fragmentPath;
};

#endif //GAMEENGINE_SHADERPROGRAM_H
