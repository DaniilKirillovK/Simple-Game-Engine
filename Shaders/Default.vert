#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
uniform mat4 uNormalMatrix;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = uModelMatrix * vec4(aPos, 1.0);
    vWorldPosition = worldPos.xyz;
    gl_Position = uProjectionMatrix * uViewMatrix * worldPos;

    vNormal = normalize(mat3(uNormalMatrix) * aNormal);
    vTexCoord = aTexCoord;
}