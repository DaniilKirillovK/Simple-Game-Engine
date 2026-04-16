#include "Loaders/MeshLoader.h"
#include "Logger.h"
#include "Resources/ResourceManager.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <stdexcept>

std::unique_ptr<Mesh> MeshLoader::LoadModel(const std::string& filepath) 
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    LOG_INFO(std::string("Loading model: ") + filepath + 
             " (meshes: " + std::to_string(scene->mNumMeshes) + 
             ", materials: " + std::to_string(scene->mNumMaterials) + ")");

    return LoadModelInner(scene);
}

std::vector<std::unique_ptr<Mesh>> MeshLoader::LoadAllMeshes(const std::string& filepath) 
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    LOG_INFO(std::string("Loading all meshes: ") + filepath + 
             " (meshes: " + std::to_string(scene->mNumMeshes) + ")");

    return LoadAllMeshesInner(scene);
}


std::unique_ptr<Mesh> MeshLoader::LoadModelInner(const aiScene* scene)
{
    return ProcessMesh(scene->mMeshes[0]);
}

std::vector<std::unique_ptr<Mesh>> MeshLoader::LoadAllMeshesInner(const aiScene* scene)
{
    std::vector<std::unique_ptr<Mesh>> meshes;
    
    meshes.reserve(scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        meshes.push_back(ProcessMesh(scene->mMeshes[i]));
    }

    LOG_INFO(std::string("Loaded ") + std::to_string(meshes.size()) + " meshes from scene");

    return meshes;
}

std::unique_ptr<Mesh> MeshLoader::ProcessMesh(struct aiMesh* mesh) 
{
    auto result = std::make_unique<Mesh>();
    result->type = MeshType::Custom;


    result->vertices.reserve(mesh->mNumVertices);
    result->normals.reserve(mesh->mNumVertices);
    result->texCoords.reserve(mesh->mNumVertices);


    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) 
    {
        glm::vec3 vertex;
        vertex.x = mesh->mVertices[i].x;
        vertex.y = mesh->mVertices[i].y;
        vertex.z = mesh->mVertices[i].z;
        result->vertices.push_back(vertex);
    }


    if (mesh->HasNormals()) {
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) 
        {
            glm::vec3 normal;
            normal.x = mesh->mNormals[i].x;
            normal.y = mesh->mNormals[i].y;
            normal.z = mesh->mNormals[i].z;
            result->normals.push_back(normal);
        }
    } 
    else 
    {
        LOG_WARNING("Mesh has no normals, using defaults");
        result->normals.resize(mesh->mNumVertices, glm::vec3(0.0f, 1.0f, 0.0f));
    }


    if (mesh->HasTextureCoords(0)) {
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) 
        {
            glm::vec2 uv;
            uv.x = mesh->mTextureCoords[0][i].x;
            uv.y = mesh->mTextureCoords[0][i].y;
            result->texCoords.push_back(uv);
        }
    } 
    else 
    {
        LOG_WARNING("Mesh has no UVs, using defaults");
        result->texCoords.resize(mesh->mNumVertices, glm::vec2(0.0f, 0.0f));
    }


    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) 
    {
        aiFace face = mesh->mFaces[i];
        
        
        if (face.mNumIndices != 3) 
        {
            LOG_WARNING(std::string("Face is not a triangle, skipping"));
            continue;
        }

        for (unsigned int j = 0; j < face.mNumIndices; ++j) 
        {
            result->indices.push_back(face.mIndices[j]);
        }
    }

    result->vertexCount = result->vertices.size();
    result->indexCount = result->indices.size();

    LOG_INFO(std::string("Processed mesh: ") + 
             std::to_string(result->vertexCount) + " vertices, " +
             std::to_string(result->indexCount) + " indices");

    return result;
}

std::string MeshLoader::ExtractDirectory(const std::string& filepath) 
{
    size_t lastSlash = filepath.find_last_of("/\\");
    return (lastSlash != std::string::npos) ? filepath.substr(0, lastSlash) : "";
}

std::unique_ptr<Mesh> MeshLoader::Load(const std::string& filepath)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    
    if (!scene ||  (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||  !scene->mRootNode) 
    {
        LOG_ERROR(std::string("Assimp Error: ") + importer.GetErrorString());
        return nullptr;
    }

    // Single mesh or multiple meshes loading
    unsigned int meshCount = scene->mNumMeshes;
    if (meshCount == 0)
    {
        LOG_ERROR("No meshes found in: " + filepath);
        return nullptr;
    }
    else if (meshCount == 1)
    {
        LOG_INFO(std::string("Single mesh detected, using LoadModelInternal: ") + filepath);
        return LoadModelInner(scene);  // 
    }
    else
    {
        LOG_INFO(std::string("Multiple meshes detected (") + std::to_string(meshCount) + "), using LoadAllMeshesInternal: " + filepath);
        auto meshes = LoadAllMeshesInner(scene);  // 
        
        if (meshes.empty())
        {
            LOG_ERROR("Failed to load meshes from: " + filepath);
            return nullptr;
        }

        // All meshes into one mesh
        auto mergedMesh = std::make_unique<Mesh>();
        mergedMesh->type = MeshType::Custom;

        unsigned int vertexOffset = 0;

        for (const auto& mesh : meshes)
        {
            mergedMesh->vertices.insert(mergedMesh->vertices.end(), mesh->vertices.begin(), mesh->vertices.end());
            mergedMesh->normals.insert(mergedMesh->normals.end(), mesh->normals.begin(), mesh->normals.end());
            mergedMesh->texCoords.insert(mergedMesh->texCoords.end(), mesh->texCoords.begin(), mesh->texCoords.end());

            // Indicies with offset
            for (unsigned int idx : mesh->indices)
            {
                mergedMesh->indices.push_back(idx + vertexOffset);
            }

            vertexOffset += mesh->vertexCount;
        }

        mergedMesh->vertexCount = mergedMesh->vertices.size();
        mergedMesh->indexCount = mergedMesh->indices.size();

        LOG_INFO(std::string("Merged ") + std::to_string(meshes.size()) + " meshes into: " +
                 std::to_string(mergedMesh->vertexCount) + " vertices, " +
                 std::to_string(mergedMesh->indexCount) + " indices");

        return mergedMesh;
    }
}

void MeshLoader::RegisterLoader()
{
    RESOURCE_MANAGER.registerLoader<Mesh>(&MeshLoader::Load, 0);
    LOG_INFO("MeshLoader registered with ResourceManager");
}
