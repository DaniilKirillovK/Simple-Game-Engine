#include "Utils/Serialization/SceneSerializer.h"
#include <fstream>
#include <iostream>
#include "Logger.h"
#include <set>
#include "Components/Material.h"
#include "Resources/ResourceManager.h"
#include "Resources/AssetLoader.h" // асинхронные запросы мешей и текстур при загрузке сцены
#include "MeshFactory.h"
#include "tracy/Tracy.hpp"

bool SceneSerializer::saveScene(World& world, const std::string& filepath)
{
    try 
    {
        nlohmann::json j = serializeWorld(world);
        std::ofstream file("../../../" + filepath);
        if (!file.is_open()) 
        {
            LOG_ERROR("Failed to open file for writing: " + filepath);
            return false;
        }

        file << j.dump(4);
        file.close();

        LOG_INFO("Scene saved to: " + filepath);
        return true;
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("Failed to save scene: " + std::string(e.what()));
        return false;
    }
}

bool SceneSerializer::loadScene(IRenderAdapter& renderAdapter, World& world, const std::string& filepath)
{
    // TRACY: главная метрика для демонстрации. Время загрузки сцены в главном потоке:
    // «до» — длинное (внутри синхронно грузятся все меши и текстуры), «после» — короткое
    // (только разбор JSON и отправка запросов). Вызывается и при старте, и при выходе из Play-режима.
    ZoneScopedN("SceneSerializer::loadScene");
    ZoneText(filepath.c_str(), filepath.size());

    try
    {
        std::ifstream file("../../../" + filepath);
        if (!file.is_open()) 
        {
            LOG_ERROR("Failed to open file for reading: " + filepath);
            return false;
        }

        world.clear();

        nlohmann::json j;
        file >> j;
        file.close();

        deserializeWorld(renderAdapter, world, j);
        LOG_INFO("Scene loaded from: " + filepath);
        return true;
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("Failed to load scene: " + std::string(e.what()));
        return false;
    }
}

nlohmann::json SceneSerializer::serializeWorld(World& world)
{
    nlohmann::json j;
    j["version"] = 1;

    auto& tPool = world.getComponentPool<Transform>();
    auto& tagPool = world.getComponentPool<Tag>();
    auto& lPool = world.getComponentPool<Light>();
    auto& mrPool = world.getComponentPool<MeshRenderer>();
    auto& rbPool = world.getComponentPool<Rigidbody>();
    auto& cPool = world.getComponentPool<Collider>();
    auto& hPool = world.getComponentPool<Hierarchy>();
    auto& camPool = world.getComponentPool<Camera>();

    std::vector<EntityId> allEntities;
    allEntities.reserve(
        tPool.size() + tagPool.size() + lPool.size() + mrPool.size() +
        rbPool.size() + cPool.size() + hPool.size() + camPool.size());
    auto append = [&](auto& pool) {
        for (EntityId e : pool.entities())
            allEntities.push_back(e);
        };
    append(tPool); append(tagPool); append(lPool); append(mrPool);
    append(rbPool); append(cPool); append(hPool); append(camPool);

    std::sort(allEntities.begin(), allEntities.end());
    allEntities.erase(std::unique(allEntities.begin(), allEntities.end()),
        allEntities.end());

    nlohmann::json entitiesJson = nlohmann::json::array();
    entitiesJson.get_ref<nlohmann::json::array_t&>().reserve(allEntities.size());

    for (EntityId e : allEntities)
    {
        nlohmann::json ej;
        ej["id"] = e;

        if (auto* t = tPool.getComponent(e))
        {
            ej["transform"] = serializeTransform(*t);
        }
        if (auto* t = tagPool.getComponent(e))
        {
            ej["tag"] = serializeTag(*t);
        }
        if (auto* t = mrPool.getComponent(e))
        {
            ej["mesh_renderer"] = serializeMeshRenderer(*t);
        }
        if (auto* t = rbPool.getComponent(e))
        {
            ej["rigidbody"] = serializeRigidbody(*t);
        }
        if (auto* t = cPool.getComponent(e))
        {
            ej["collider"] = serializeCollider(*t);
        }
        if (auto* t = hPool.getComponent(e))
        {
            ej["hierarchy"] = serializeHierarchy(*t);
        }
        if (auto* t = lPool.getComponent(e))
        {
            ej["light"] = serializeLight(*t);
        }
        if (auto* t = camPool.getComponent(e))
        {
            ej["camera"] = serializeCamera(*t);
        }

        entitiesJson.push_back(std::move(ej));
    }

    j["entities"] = std::move(entitiesJson);
    return j;
}

void SceneSerializer::deserializeWorld(IRenderAdapter& renderAdapter, World& world, const nlohmann::json& j)
{
    struct EntityData 
    {
        EntityId oldId;
        const nlohmann::json* transform = nullptr;
        const nlohmann::json* tag = nullptr;
        const nlohmann::json* light = nullptr;
        const nlohmann::json* meshRenderer = nullptr;
        const nlohmann::json* rigidbody = nullptr;
        const nlohmann::json* collider = nullptr;
        const nlohmann::json* hierarchy = nullptr;
        const nlohmann::json* camera = nullptr;
    };

    const auto& entitiesArray = j["entities"];
    std::vector<EntityData> entitiesData;
    entitiesData.reserve(entitiesArray.size());

    std::unordered_map<EntityId, EntityId> idMap;
    idMap.reserve(entitiesArray.size() * 2);

    for (const auto& entityJson : entitiesArray)
    {
        EntityId oldId = entityJson["id"].get<EntityId>();
        EntityId newId = world.createEntity();
        idMap[oldId] = newId;

        EntityData d;
        d.oldId = oldId;
        if (entityJson.contains("transform"))     d.transform = &entityJson["transform"];
        if (entityJson.contains("tag"))           d.tag = &entityJson["tag"];
        if (entityJson.contains("light"))         d.light = &entityJson["light"];
        if (entityJson.contains("mesh_renderer")) d.meshRenderer = &entityJson["mesh_renderer"];
        if (entityJson.contains("rigidbody"))     d.rigidbody = &entityJson["rigidbody"];
        if (entityJson.contains("collider"))      d.collider = &entityJson["collider"];
        if (entityJson.contains("hierarchy"))     d.hierarchy = &entityJson["hierarchy"];
        if (entityJson.contains("camera"))        d.camera = &entityJson["camera"];

        entitiesData.push_back(d);
    }

    auto& tPool = world.getComponentPool<Transform>();
    auto& tagPool = world.getComponentPool<Tag>();
    auto& lPool = world.getComponentPool<Light>();
    auto& mrPool = world.getComponentPool<MeshRenderer>();
    auto& rbPool = world.getComponentPool<Rigidbody>();
    auto& cPool = world.getComponentPool<Collider>();
    auto& hPool = world.getComponentPool<Hierarchy>();
    auto& camPool = world.getComponentPool<Camera>();

    for (const auto& d : entitiesData)
    {
        EntityId newId = idMap[d.oldId];

        if (d.transform)
        {
            Transform* t = tPool.getComponent(newId);
            if (!t) { tPool.addComponent(newId, Transform{}); t = tPool.getComponent(newId); }
            deserializeTransform(*t, *d.transform);
        }

        if (d.tag)
        {
            Tag* t = tagPool.getComponent(newId);
            if (!t) { tagPool.addComponent(newId, Tag{}); t = tagPool.getComponent(newId); }
            deserializeTag(*t, *d.tag);
        }

        if (d.light)
        {
            Light* l = lPool.getComponent(newId);
            if (!l) { lPool.addComponent(newId, Light{}); l = lPool.getComponent(newId); }
            deserializeLight(*l, *d.light);
        }

        if (d.meshRenderer)
        {
            MeshRenderer* m = mrPool.getComponent(newId);
            if (!m) { mrPool.addComponent(newId, MeshRenderer{}); m = mrPool.getComponent(newId); }
            deserializeMeshRenderer(renderAdapter, *m, *d.meshRenderer);
        }

        if (d.rigidbody)
        {
            Rigidbody* r = rbPool.getComponent(newId);
            if (!r) { rbPool.addComponent(newId, Rigidbody{}); r = rbPool.getComponent(newId); }
            deserializeRigidbody(*r, *d.rigidbody);
        }

        if (d.collider)
        {
            Collider* c = cPool.getComponent(newId);
            if (!c) { cPool.addComponent(newId, Collider{}); c = cPool.getComponent(newId); }
            deserializeCollider(*c, *d.collider);
        }

        if (d.hierarchy)
        {
            Hierarchy* h = hPool.getComponent(newId);
            if (!h) { hPool.addComponent(newId, Hierarchy{}); h = hPool.getComponent(newId); }
            deserializeHierarchy(*h, *d.hierarchy);
        }

        if (d.camera)
        {
            Camera* c = camPool.getComponent(newId);
            if (!c) { camPool.addComponent(newId, Camera{}); c = camPool.getComponent(newId); }
            deserializeCamera(*c, *d.camera);
        }
    }

    for (const auto& d : entitiesData)
    {
        if (!d.hierarchy) continue;

        EntityId newId = idMap[d.oldId];
        Hierarchy* h = hPool.getComponent(newId);
        if (!h || h->parent == INVALID_ENTITY) continue;

        auto it = idMap.find(h->parent);
        if (it == idMap.end()) continue;

        EntityId newParent = it->second;
        h->parent = newParent;

        if (auto* ph = hPool.getComponent(newParent))
        {
            ph->children.push_back(newId);
        }
    }

    LOG_INFO("Scene loaded, entities: " + std::to_string(tPool.size()));
}

nlohmann::json SceneSerializer::serializeTransform(const Transform& transform)
{
	nlohmann::json j;
	j["position"] = { transform.position.x, transform.position.y, transform.position.z };

	glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
	j["rotation"] = { euler.x, euler.y, euler.z };

	j["scale"] = { transform.scale.x, transform.scale.y, transform.scale.z };
	return j;
}

nlohmann::json SceneSerializer::serializeTag(const Tag& tag)
{
	nlohmann::json j;
	j["name"] = tag.name;
	return j;
}

nlohmann::json SceneSerializer::serializeLight(const Light& light)
{
    nlohmann::json j;

    std::string typeStr;
    switch (light.type) 
    {
    case LightType::Directional: 
        typeStr = "directional"; 
        break;
    case LightType::Point: 
        typeStr = "point"; 
        break;
    case LightType::Spot: 
        typeStr = "spot"; 
        break;
    }
    j["type"] = typeStr;

    j["color"] = { light.color.r, light.color.g, light.color.b };
    j["intensity"] = light.intensity;
    j["enabled"] = light.enabled;

    j["position"] = { light.position.x, light.position.y, light.position.z, light.position.w };

    // Directional light
    j["direction"] = { light.direction.x, light.direction.y, light.direction.z };

    // Point light
    j["constant"] = light.constant;
    j["linear"] = light.linear;
    j["quadratic"] = light.quadratic;
    j["range"] = light.range;

    // Spot light
    j["cut_off"] = light.cutOff;
    j["outer_cut_off"] = light.outerCutOff;

    return j;
}

nlohmann::json SceneSerializer::serializeMeshRenderer(const MeshRenderer& renderer)
{
    nlohmann::json j;
    j["visible"] = renderer.visible;
    j["render_layer"] = renderer.renderLayer;

    if (renderer.mesh) 
    {
        nlohmann::json meshJson;

        if (!renderer.mesh->path.empty()) 
        {
            meshJson["path"] = renderer.mesh->path;
            meshJson["type"] = "model";
        }
        else 
        {
            switch (renderer.mesh->type) {
            case MeshType::Cube:
                meshJson["type"] = "cube";
                break;
            case MeshType::Sphere:
                meshJson["type"] = "sphere";
                meshJson["radius"] = 0.5f;
                break;
            case MeshType::Plane:
                meshJson["type"] = "plane";
                break;
            }
        }

        j["mesh"] = meshJson;
    }

    if (renderer.material) 
    {
        j["material"]["diffuse_color"] = {
            renderer.material->diffuseColor.r,
            renderer.material->diffuseColor.g,
            renderer.material->diffuseColor.b,
            renderer.material->diffuseColor.a
        };
        j["material"]["specular_color"] = {
            renderer.material->specularColor.r,
            renderer.material->specularColor.g,
            renderer.material->specularColor.b,
            renderer.material->specularColor.a
        };
        j["material"]["ambient_color"] = {
            renderer.material->ambientColor.r,
            renderer.material->ambientColor.g,
            renderer.material->ambientColor.b,
            renderer.material->ambientColor.a
        };
        j["material"]["shininess"] = renderer.material->shininess;
        j["material"]["has_texture"] = renderer.material->hasTexture;

        j["material"]["vertex_shader_path"] = renderer.material->vertexShaderPath;
        j["material"]["fragment_shader_path"] = renderer.material->fragmentShaderPath;

        if (renderer.material->hasTexture && renderer.material->diffuseTexture) 
        {
            j["material"]["diffuse_texture_path"] = renderer.material->diffuseTexture->m_path;
        }
    }

    return j;
}

nlohmann::json SceneSerializer::serializeRigidbody(const Rigidbody& rb)
{
    nlohmann::json j;
    j["mass"] = rb.mass;
    j["use_gravity"] = rb.useGravity;
    j["is_kinematic"] = rb.isKinematic;
    j["velocity"] = { rb.velocity.x, rb.velocity.y, rb.velocity.z };
    return j;
}

nlohmann::json SceneSerializer::serializeCollider(const Collider& collider)
{
    nlohmann::json j;
    j["type"] = collider.type == ColliderType::Box ? "box" : "sphere";
    j["offset"] = { collider.offset.x, collider.offset.y, collider.offset.z };
    j["bounciness"] = collider.bounciness;
    j["friction"] = collider.friction;
    j["is_trigger"] = collider.isTrigger;

    if (collider.type == ColliderType::Box) 
    {
        j["half_size"] = { collider.halfSize.x, collider.halfSize.y, collider.halfSize.z };
    }
    else 
    {
        j["radius"] = collider.radius;
    }

    return j;
}

nlohmann::json SceneSerializer::serializeHierarchy(const Hierarchy& hierarchy)
{
    nlohmann::json j;
    j["parent"] = hierarchy.parent;
    return j;
}

nlohmann::json SceneSerializer::serializeCamera(const Camera& camera)
{
    nlohmann::json j;
    j["forward"] = { camera.forward.x, camera.forward.y, camera.forward.z };
    j["up"] = { camera.up.x, camera.up.y, camera.up.z };
    j["right"] = { camera.right.x, camera.right.y, camera.right.z };

    j["fov"] = camera.fov;
    j["aspect_ratio"] = camera.aspectRatio;
    j["near_plane"] = camera.nearPlane;
    j["far_plane"] = camera.farPlane;
    j["is_active"] = camera.isActive;
    return j;
}

void SceneSerializer::deserializeTransform(Transform& transform, const nlohmann::json& j)
{
    if (j.contains("position")) 
    {
        transform.position = glm::vec3(j["position"][0], j["position"][1], j["position"][2]);
    }
    if (j.contains("rotation")) 
    {
        glm::vec3 euler(j["rotation"][0], j["rotation"][1], j["rotation"][2]);
        transform.rotation = glm::quat(glm::radians(euler));
        transform.eulerRotation = euler;
    }
    if (j.contains("scale")) 
    {
        transform.scale = glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]);
    }
    transform.markDirty();
}

void SceneSerializer::deserializeTag(Tag& tag, const nlohmann::json& j)
{
    if (j.contains("name")) 
    {
        tag.name = j["name"].get<std::string>();
    }
}

void SceneSerializer::deserializeLight(Light& light, const nlohmann::json& j)
{
    if (j.contains("type")) 
    {
        std::string typeStr = j["type"].get<std::string>();
        if (typeStr == "directional") light.type = LightType::Directional;
        else if (typeStr == "point") light.type = LightType::Point;
        else if (typeStr == "spot") light.type = LightType::Spot;
    }

    // Общие свойства
    if (j.contains("color")) 
    {
        light.color = glm::vec3(j["color"][0], j["color"][1], j["color"][2]);
    }
    if (j.contains("intensity")) light.intensity = j["intensity"].get<float>();
    if (j.contains("enabled")) light.enabled = j["enabled"].get<bool>();

    if (j.contains("position")) 
    {
        light.position = glm::vec4(j["position"][0], j["position"][1],
            j["position"][2], j["position"][3]);
    }

    // Directional light
    if (j.contains("direction")) 
    {
        light.direction = glm::vec3(j["direction"][0], j["direction"][1], j["direction"][2]);
    }

    // Point light
    if (j.contains("constant")) light.constant = j["constant"].get<float>();
    if (j.contains("linear")) light.linear = j["linear"].get<float>();
    if (j.contains("quadratic")) light.quadratic = j["quadratic"].get<float>();
    if (j.contains("range")) light.range = j["range"].get<float>();

    // Spot light
    if (j.contains("cut_off")) light.cutOff = j["cut_off"].get<float>();
    if (j.contains("outer_cut_off")) light.outerCutOff = j["outer_cut_off"].get<float>();
}

void SceneSerializer::deserializeMeshRenderer(IRenderAdapter& renderAdapter, MeshRenderer& renderer, const nlohmann::json& j)
{
    if (j.contains("visible")) renderer.visible = j["visible"].get<bool>();
    if (j.contains("render_layer")) renderer.renderLayer = j["render_layer"].get<int>();

    if (j.contains("mesh")) 
    {
        auto& meshJson = j["mesh"];

        if (meshJson.contains("path") && !meshJson["path"].get<std::string>().empty()) 
        {
            std::string meshPath = meshJson["path"].get<std::string>();
            // БЫЛО: RESOURCE_MANAGER.load<Mesh>(meshPath) — синхронная загрузка: пока Assimp
            // разбирал модель, главный поток стоял, и загрузка сцены с несколькими тяжёлыми
            // моделями замораживала кадры.
            // СТАЛО: requestMesh возвращает ресурс МГНОВЕННО. Внутри лежит пустая заглушка
            // (ready == false), которую RenderSystem пропускает. Настоящие данные декодируются
            // в воркере и подставляются в этот же объект на месте через AssetLoader::pump.
            // Указатель renderer.mesh остаётся валидным всё время, менять его позже не нужно.
            auto meshResource = AssetLoader::getInstance().requestMesh(meshPath);
            if (meshResource && meshResource->isValid())
            {
                renderer.mesh = meshResource->get();
                LOG_INFO("Requested mesh: " + meshPath);
            }
            else
            {
                LOG_WARNING("Failed to request mesh: " + meshPath);
            }
        }
        else if (meshJson.contains("type")) 
        {
            renderer.mesh = createPrimitiveMesh(meshJson);
        }
    }

    if (j.contains("material")) 
    {
        auto& mat = j["material"];

        glm::vec4 diffuseColor(1.0f);
        glm::vec4 specularColor(0.5f);
        glm::vec4 ambientColor(0.2f);
        float shininess = 32.0f;

        if (mat.contains("diffuse_color")) 
        {
            diffuseColor = glm::vec4(
                mat["diffuse_color"][0], mat["diffuse_color"][1],
                mat["diffuse_color"][2], mat["diffuse_color"][3]
            );
        }
        if (mat.contains("specular_color")) 
        {
            specularColor = glm::vec4(
                mat["specular_color"][0], mat["specular_color"][1],
                mat["specular_color"][2], mat["specular_color"][3]
            );
        }
        if (mat.contains("ambient_color"))
        {
            ambientColor = glm::vec4(
                mat["ambient_color"][0], mat["ambient_color"][1],
                mat["ambient_color"][2], mat["ambient_color"][3]
            );
        }
        if (mat.contains("shininess")) 
        {
            shininess = mat["shininess"].get<float>();
        }

        // Шейдеры остаются СИНХРОННЫМИ: они маленькие и их мало, а конструктор Material сразу
        // линкует программу и запрашивает uniform-ы, то есть ему нужны уже скомпилированные
        // GL-идентификаторы. Чтобы сделать их асинхронными, пришлось бы переделать Material.
        // ИСПРАВЛЕНИЕ: раньше было load<Shader>(...)->get() без проверки. Если шейдер не
        // найден или не скомпилировался, load возвращал nullptr, и вызов ->get() падал.
        Shader* vertShader = nullptr;
        Shader* fragShader = nullptr;
        if (mat.contains("vertex_shader_path"))
        {
            auto shaderResource = RESOURCE_MANAGER.load<Shader>(mat["vertex_shader_path"].get<std::string>());
            if (shaderResource) vertShader = shaderResource->get();
        }
        if (mat.contains("fragment_shader_path"))
        {
            auto shaderResource = RESOURCE_MANAGER.load<Shader>(mat["fragment_shader_path"].get<std::string>());
            if (shaderResource) fragShader = shaderResource->get();
        }

        // Без обоих шейдеров материал создать нельзя (конструктор Material разыменует
        // указатели). Раньше это было падением; теперь материал пропускается с записью в лог,
        // а у объекта renderer.material остаётся nullptr, и RenderSystem такой объект не рисует.
        if (!vertShader || !fragShader)
        {
            LOG_ERROR("Scene material skipped: vertex/fragment shader is missing or failed to compile");
            return;
        }

        Material* material = new Material{
            diffuseColor, vertShader, fragShader, renderAdapter
        };

        if (mat.contains("has_texture") && mat["has_texture"].get<bool>()) 
        {
            if (mat.contains("diffuse_texture_path")) 
            {
                std::string texturePath = mat["diffuse_texture_path"].get<std::string>();
                // БЫЛО: load<Texture> — синхронное декодирование (stb_image) и загрузка в GPU
                // прямо во время загрузки сцены.
                // СТАЛО: requestTexture сразу отдаёт объект Texture, который пока указывает на
                // белую заглушку 1x1. Декодирование идёт в воркере, а когда картинка загрузится
                // в GPU, AssetLoader подменит handle в ЭТОМ ЖЕ объекте. Поэтому material->diffuseTexture
                // менять не нужно: материал сам «подхватит» настоящую текстуру.
                auto textureResource = AssetLoader::getInstance().requestTexture(texturePath);
                if (textureResource && textureResource->isValid())
                {
                    material->diffuseTexture = textureResource->get();
                    material->hasTexture = true;
                    LOG_INFO("Requested texture: " + texturePath);
                }
            }
        }

        renderer.material = material;
    }
}

void SceneSerializer::deserializeRigidbody(Rigidbody& rb, const nlohmann::json& j)
{
    if (j.contains("mass")) rb.mass = j["mass"].get<float>();
    if (j.contains("use_gravity")) rb.useGravity = j["use_gravity"].get<bool>();
    if (j.contains("is_kinematic")) rb.isKinematic = j["is_kinematic"].get<bool>();
    if (j.contains("velocity")) 
    {
        rb.velocity = glm::vec3(j["velocity"][0], j["velocity"][1], j["velocity"][2]);
    }
    rb.updateInvMass();
}

void SceneSerializer::deserializeCollider(Collider& collider, const nlohmann::json& j)
{
    if (j.contains("type")) 
    {
        std::string type = j["type"].get<std::string>();
        collider.type = (type == "box") ? ColliderType::Box : ColliderType::Sphere;
    }
    if (j.contains("offset")) 
    {
        collider.offset = glm::vec3(j["offset"][0], j["offset"][1], j["offset"][2]);
    }
    if (j.contains("bounciness")) collider.bounciness = j["bounciness"].get<float>();
    if (j.contains("friction")) collider.friction = j["friction"].get<float>();
    if (j.contains("is_trigger")) collider.isTrigger = j["is_trigger"].get<bool>();

    if (collider.type == ColliderType::Box && j.contains("half_size")) 
    {
        collider.halfSize = glm::vec3(j["half_size"][0], j["half_size"][1], j["half_size"][2]);
    }
    else if (collider.type == ColliderType::Sphere && j.contains("radius")) 
    {
        collider.radius = j["radius"].get<float>();
    }
}

void SceneSerializer::deserializeHierarchy(Hierarchy& hierarchy, const nlohmann::json& j)
{
    if (j.contains("parent")) 
    {
        hierarchy.parent = j["parent"].get<EntityId>();
    }
}

void SceneSerializer::deserializeCamera(Camera& camera, const nlohmann::json& j)
{
    if (j.contains("forward"))
    {
        camera.forward = glm::vec3(j["forward"][0], j["forward"][1], j["forward"][2]);
    }
    if (j.contains("up"))
    {
        camera.up = glm::vec3(j["up"][0], j["up"][1], j["up"][2]);
    }
    if (j.contains("right"))
    {
        camera.right = glm::vec3(j["right"][0], j["right"][1], j["right"][2]);
    }

    if (j.contains("fov")) camera.fov = j["fov"].get<float>();
    if (j.contains("aspect_ratio")) camera.aspectRatio = j["aspect_ratio"].get<float>();
    if (j.contains("near_plane")) camera.nearPlane = j["near_plane"].get<float>();
    if (j.contains("far_plane")) camera.farPlane = j["far_plane"].get<float>();
    if (j.contains("is_active")) camera.isActive = j["is_active"].get<bool>();
}

Mesh* SceneSerializer::createPrimitiveMesh(const nlohmann::json& meshJson)
{
    std::string type = meshJson["type"].get<std::string>();

    if (type == "cube") 
    {
        return MeshFactory::createCube();
    }
    else if (type == "sphere") 
    {
        float radius = meshJson.contains("radius") ? meshJson["radius"].get<float>() : 0.5f;
        int sectors = meshJson.contains("sectors") ? meshJson["sectors"].get<int>() : 36;
        int stacks = meshJson.contains("stacks") ? meshJson["stacks"].get<int>() : 18;
        return MeshFactory::createSphere(radius, sectors, stacks);
    }
    else if (type == "plane") 
    {
        float size = meshJson.contains("size") ? meshJson["size"].get<float>() : 10.0f;
        return MeshFactory::createPlane(size);
    }

    return MeshFactory::createCube();
}
