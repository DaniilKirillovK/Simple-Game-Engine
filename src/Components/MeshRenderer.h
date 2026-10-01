#pragma once
#include "Component.h"
#include "Material.h"
#include <glm/glm.hpp>

enum class MeshType 
{
    Cube,
    Sphere,
    Plane,
    Custom
};

struct Mesh 
{
    MeshType type = MeshType::Cube;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<unsigned int> indices;

    std::string path;

    // Признак «меш полностью загружен».
    // false — меш является пустой заглушкой: AssetLoader создал объект сразу при запросе,
    // а данные (вершины, индексы) придут позже из воркера. Рисовать такой меш нельзя:
    // рендерер создал бы для него пустой VAO и закэшировал его по указателю, после чего
    // заполненный меш так и остался бы пустым на экране. RenderSystem и drawMesh пропускают
    // меши с ready == false. AssetLoader заполняет этот же объект на месте и ставит ready = true.
    // По умолчанию true: примитивы из MeshFactory и меши, загруженные синхронно, всегда готовы.
    bool ready = true;

    // OpenGL buffers
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;
};

class MeshRenderer : public Component
{
public:
    MeshRenderer() : mesh(nullptr), material(nullptr) {};
    MeshRenderer(Mesh* mesh, Material* material)
        : mesh(mesh), material(material) {}

    Mesh* mesh;
    Material* material;
    bool visible = true;
    int renderLayer = 0;
};