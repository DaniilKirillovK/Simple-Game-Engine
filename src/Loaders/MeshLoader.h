#pragma once

#include "Components/MeshRenderer.h"
#include <string>
#include <memory>
#include <vector>

// Forward declaration
struct aiScene;

class MeshLoader 
{
public:

    static std::unique_ptr<Mesh> LoadModel(const std::string& filepath);
    static std::vector<std::unique_ptr<Mesh>> LoadAllMeshes(const std::string& filepath);

    static std::unique_ptr<Mesh> Load(const std::string& filepath);

    //Register this loader with ResourceManager
    static void RegisterLoader();

private:
    // Internal methods that work with already-loaded scene
    static std::unique_ptr<Mesh> LoadModelInner(const aiScene* scene);
    static std::vector<std::unique_ptr<Mesh>> LoadAllMeshesInner(const aiScene* scene);

    // Converts aiMesh(Assimp) to Mesh(Engine)
    static std::unique_ptr<Mesh> ProcessMesh(struct aiMesh* mesh);

    //Extract Directory from file path
    static std::string ExtractDirectory(const std::string& filepath);
};
