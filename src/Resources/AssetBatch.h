#pragma once

#include "JobSystem/JobSystem.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class IRenderAdapter;
struct Mesh;
struct Texture;
struct TextureData;


class AssetBatch
{
public:
    AssetBatch(JobSystem* jobs, IRenderAdapter* renderAdapter);

    void requestMesh(const std::string& path);
    void requestTexture(const std::string& path);

    // Runs the parallel CPU stage and then the serial GPU stage.
    void flush();

    Mesh* getMesh(const std::string& path) const;
    Texture* getTexture(const std::string& path) const;

private:
    JobSystem* m_jobs = nullptr;
    IRenderAdapter* m_renderAdapter = nullptr;

    std::unordered_set<std::string> m_requestedMeshes;
    std::unordered_set<std::string> m_requestedTextures;

    std::vector<std::string> m_meshPaths;
    std::vector<std::string> m_texturePaths;

    std::vector<std::unique_ptr<Mesh>> m_meshResults;
    std::vector<std::unique_ptr<TextureData>> m_textureDataResults;

    std::unordered_map<std::string, Mesh*> m_meshCache;
    std::unordered_map<std::string, Texture*> m_textureCache;
};
