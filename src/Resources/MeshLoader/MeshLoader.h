#pragma once

#include "Components/MeshRenderer.h"
#include <string>
#include <memory>
#include <vector>

struct aiScene;

class MeshLoader 
{
public:
    MeshLoader() = default;
    ~MeshLoader() = default;

    static MeshLoader& getInstance()
    {
        static MeshLoader instance;
        return instance;
    }

    std::unique_ptr<Mesh> loadModel(const std::string& filepath);
    std::vector<std::unique_ptr<Mesh>> loadAllMeshes(const std::string& filepath);

    std::unique_ptr<Mesh> loadMesh(const std::string& filepath);

    void registerLoader();

private:
    std::unique_ptr<Mesh> loadModelInner(const aiScene* scene);
    std::vector<std::unique_ptr<Mesh>> loadAllMeshesInner(const aiScene* scene);

    std::unique_ptr<Mesh> processMesh(struct aiMesh* mesh);

    std::string extractDirectory(const std::string& filepath);
};


inline std::unique_ptr<Mesh> meshLoad(const std::string& path)
{
    return MeshLoader::getInstance().loadMesh(path);
}
