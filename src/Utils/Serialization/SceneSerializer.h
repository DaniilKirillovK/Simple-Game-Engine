#pragma once
#include "Utils/json.hpp"
#include "World.h"
#include "Components/Transform.h"
#include "Components/Tag.h"
#include "Components/MeshRenderer.h"
#include "Components/Rigidbody.h"
#include "Components/Collider.h"
#include "Components/Hierarchy.h"
#include "Components/Light.h"
#include "Components/Camera.h"
#include "IRenderAdapter.h"
#include <string>



class SceneSerializer 
{
public:
    static bool saveScene(World& world, const std::string& filepath);
    static bool loadScene(IRenderAdapter& renderAdapter, World& world, const std::string& filepath);
    static nlohmann::json serializeWorld(World& world);
    static void deserializeWorld(IRenderAdapter& renderAdapter, World& world, const nlohmann::json& j);

private:
    static nlohmann::json serializeTransform(const Transform& transform);
    static nlohmann::json serializeTag(const Tag& tag);
    static nlohmann::json serializeLight(const Light& light);
    static nlohmann::json serializeMeshRenderer(const MeshRenderer& renderer);
    static nlohmann::json serializeRigidbody(const Rigidbody& rb);
    static nlohmann::json serializeCollider(const Collider& collider);
    static nlohmann::json serializeHierarchy(const Hierarchy& hierarchy);
    static nlohmann::json serializeCamera(const Camera& camera);

    static void deserializeTransform(Transform& transform, const nlohmann::json& j);
    static void deserializeTag(Tag& tag, const nlohmann::json& j);
    static void deserializeLight(Light& light, const nlohmann::json& j);
    static void deserializeMeshRenderer(IRenderAdapter& renderAdapter, MeshRenderer& renderer, const nlohmann::json& j);
    static void deserializeRigidbody(Rigidbody& rb, const nlohmann::json& j);
    static void deserializeCollider(Collider& collider, const nlohmann::json& j);
    static void deserializeHierarchy(Hierarchy& hierarchy, const nlohmann::json& j);
    static void deserializeCamera(Camera& camera, const nlohmann::json& j);

    static Mesh* createPrimitiveMesh(const nlohmann::json& meshJson);
};