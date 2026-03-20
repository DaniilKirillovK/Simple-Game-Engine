#pragma once
#include "Component.h"
#include <vector>
#include <cmath>
#include "Components/MeshRenderer.h"

class MeshFactory 
{
public:
    static Mesh* createCube() 
    {
        Mesh* mesh = new Mesh();
        mesh->type = MeshType::Cube;

        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<unsigned int> indices;

        vertices.push_back(glm::vec3(-0.5f, -0.5f, 0.5f)); // 0
        vertices.push_back(glm::vec3(0.5f, -0.5f, 0.5f)); // 1
        vertices.push_back(glm::vec3(0.5f, 0.5f, 0.5f)); // 2
        vertices.push_back(glm::vec3(-0.5f, 0.5f, 0.5f)); // 3
        for (int i = 0; i < 4; i++) normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
        texCoords.insert(texCoords.end(), {
            glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
            });
        indices.push_back(0); indices.push_back(1); indices.push_back(2);
        indices.push_back(2); indices.push_back(3); indices.push_back(0);

        int base = vertices.size();
        vertices.push_back(glm::vec3(-0.5f, -0.5f, -0.5f)); // 4
        vertices.push_back(glm::vec3(0.5f, -0.5f, -0.5f)); // 5
        vertices.push_back(glm::vec3(0.5f, 0.5f, -0.5f)); // 6
        vertices.push_back(glm::vec3(-0.5f, 0.5f, -0.5f)); // 7
        for (int i = 0; i < 4; i++) normals.push_back(glm::vec3(0.0f, 0.0f, -1.0f));
        texCoords.insert(texCoords.end(), {
            glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
            });
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 0); indices.push_back(base + 3);

        base = vertices.size();
        vertices.push_back(glm::vec3(-0.5f, 0.5f, 0.5f)); // 8
        vertices.push_back(glm::vec3(0.5f, 0.5f, 0.5f)); // 9
        vertices.push_back(glm::vec3(0.5f, 0.5f, -0.5f)); // 10
        vertices.push_back(glm::vec3(-0.5f, 0.5f, -0.5f)); // 11
        for (int i = 0; i < 4; i++) normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
        texCoords.insert(texCoords.end(), {
            glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
            });
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base + 0);

        base = vertices.size();
        vertices.push_back(glm::vec3(-0.5f, -0.5f, 0.5f)); // 12
        vertices.push_back(glm::vec3(0.5f, -0.5f, 0.5f)); // 13
        vertices.push_back(glm::vec3(0.5f, -0.5f, -0.5f)); // 14
        vertices.push_back(glm::vec3(-0.5f, -0.5f, -0.5f)); // 15
        for (int i = 0; i < 4; i++) normals.push_back(glm::vec3(0.0f, -1.0f, 0.0f));
        texCoords.insert(texCoords.end(), {
            glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
            });
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 0); indices.push_back(base + 3);

        base = vertices.size();
        vertices.push_back(glm::vec3(0.5f, -0.5f, 0.5f)); // 16
        vertices.push_back(glm::vec3(0.5f, -0.5f, -0.5f)); // 17
        vertices.push_back(glm::vec3(0.5f, 0.5f, -0.5f)); // 18
        vertices.push_back(glm::vec3(0.5f, 0.5f, 0.5f)); // 19
        for (int i = 0; i < 4; i++) normals.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
        texCoords.insert(texCoords.end(), {
            glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
            });
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base + 0);

        base = vertices.size();
        vertices.push_back(glm::vec3(-0.5f, -0.5f, 0.5f)); // 20
        vertices.push_back(glm::vec3(-0.5f, -0.5f, -0.5f)); // 21
        vertices.push_back(glm::vec3(-0.5f, 0.5f, -0.5f)); // 22
        vertices.push_back(glm::vec3(-0.5f, 0.5f, 0.5f)); // 23
        for (int i = 0; i < 4; i++) normals.push_back(glm::vec3(-1.0f, 0.0f, 0.0f));
        texCoords.insert(texCoords.end(), {
            glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
            });
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 0); indices.push_back(base + 3);

        mesh->vertices = vertices;
        mesh->normals = normals;
        mesh->texCoords = texCoords;
        mesh->indices = indices;
        mesh->vertexCount = vertices.size();
        mesh->indexCount = indices.size();

        return mesh;
    }

    static Mesh* createSphere(float radius, int sectors, int stacks) 
    {
        Mesh* mesh = new Mesh();
        mesh->type = MeshType::Sphere;

        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<unsigned int> indices;

        float x, y, z, xy;
        float nx, ny, nz;
        float s, t;

        for (int i = 0; i <= stacks; ++i) 
        {
            float v = (float)i / (float)stacks;
            float phi = v * glm::pi<float>();

            for (int j = 0; j <= sectors; ++j) 
            {
                float u = (float)j / (float)sectors;
                float theta = u * 2.0f * glm::pi<float>();

                x = radius * sin(phi) * cos(theta);
                y = radius * cos(phi);
                z = radius * sin(phi) * sin(theta);

                nx = x / radius;
                ny = y / radius;
                nz = z / radius;

                s = u;
                t = v;

                vertices.push_back(glm::vec3(x, y, z));
                normals.push_back(glm::vec3(nx, ny, nz));
                texCoords.push_back(glm::vec2(s, t));
            }
        }

        for (int i = 0; i < stacks; ++i) 
        {
            for (int j = 0; j < sectors; ++j) 
            {
                int first = i * (sectors + 1) + j;
                int second = first + sectors + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        mesh->vertices = vertices;
        mesh->normals = normals;
        mesh->texCoords = texCoords;
        mesh->indices = indices;
        mesh->vertexCount = vertices.size();
        mesh->indexCount = indices.size();

        return mesh;
    }

    static Mesh* createPlane(float size) 
    {
        Mesh* mesh = new Mesh();
        mesh->type = MeshType::Plane;

        float half = size / 2.0f;

        std::vector<glm::vec3> vertices = 
        {
            glm::vec3(-half, 0.0f, -half),
            glm::vec3(half, 0.0f, -half),
            glm::vec3(half, 0.0f,  half),
            glm::vec3(-half, 0.0f,  half)
        };

        std::vector<glm::vec3> normals = 
        {
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        };

        std::vector<glm::vec2> texCoords = 
        {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(0.0f, 1.0f)
        };

        std::vector<unsigned int> indices = 
        {
            0, 1, 2,
            2, 3, 0
        };

        mesh->vertices = vertices;
        mesh->normals = normals;
        mesh->texCoords = texCoords;
        mesh->indices = indices;
        mesh->vertexCount = vertices.size();
        mesh->indexCount = indices.size();

        return mesh;
    }
};