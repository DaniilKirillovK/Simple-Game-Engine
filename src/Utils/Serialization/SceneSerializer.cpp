#include "Utils/Serialization/SceneSerializer.h"
#include <fstream>
#include <iostream>
#include "Logger.h"
#include <set>
#include "Components/Material.h"
#include "Resources/ResourceManager.h"
#include "MeshFactory.h"

bool SceneSerializer::saveScene(World& world, const std::string& filepath)
{
    try 
    {
        nlohmann::json j = serializeWorld(world);
        std::ofstream file(filepath);
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
    try 
    {
        std::ifstream file(filepath);
        if (!file.is_open()) 
        {
            LOG_ERROR("Failed to open file for reading: " + filepath);
            return false;
        }

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
    j["entities"] = nlohmann::json::array();

    auto& transforms = world.getComponentPool<Transform>();
    auto& tags = world.getComponentPool<Tag>();
    auto& lights = world.getComponentPool<Light>();
    auto& meshRenderers = world.getComponentPool<MeshRenderer>();
    auto& rigidbodies = world.getComponentPool<Rigidbody>();
    auto& colliders = world.getComponentPool<Collider>();
    auto& hierarchies = world.getComponentPool<Hierarchy>();

    std::set<EntityId> allEntities;

    for (auto& [entity, _] : transforms.getAll()) allEntities.insert(entity);
    for (auto& [entity, _] : tags.getAll()) allEntities.insert(entity);
    for (auto& [entity, _] : lights.getAll()) allEntities.insert(entity);
    for (auto& [entity, _] : meshRenderers.getAll()) allEntities.insert(entity);
    for (auto& [entity, _] : rigidbodies.getAll()) allEntities.insert(entity);
    for (auto& [entity, _] : colliders.getAll()) allEntities.insert(entity);
    for (auto& [entity, _] : hierarchies.getAll()) allEntities.insert(entity);

    for (EntityId entity : allEntities) 
    {
        nlohmann::json entityJson;
        entityJson["id"] = entity;

        if (transforms.hasComponent(entity)) 
        {
            entityJson["transform"] = serializeTransform(*transforms.getComponent(entity));
        }
        if (tags.hasComponent(entity)) 
        {
            entityJson["tag"] = serializeTag(*tags.getComponent(entity));
        }
        if (meshRenderers.hasComponent(entity)) 
        {
            entityJson["mesh_renderer"] = serializeMeshRenderer(*meshRenderers.getComponent(entity));
        }
        if (rigidbodies.hasComponent(entity)) 
        {
            entityJson["rigidbody"] = serializeRigidbody(*rigidbodies.getComponent(entity));
        }
        if (colliders.hasComponent(entity)) 
        {
            entityJson["collider"] = serializeCollider(*colliders.getComponent(entity));
        }
        if (hierarchies.hasComponent(entity)) 
        {
            entityJson["hierarchy"] = serializeHierarchy(*hierarchies.getComponent(entity));
        }
        if (lights.hasComponent(entity)) 
        {
            entityJson["light"] = serializeLight(*lights.getComponent(entity));
        }

        j["entities"].push_back(entityJson);
    }

    return j;
}

void SceneSerializer::deserializeWorld(IRenderAdapter& renderAdapter, World& world, const nlohmann::json& j)
{
    struct EntityData 
    {
        EntityId oldId;
        nlohmann::json transform;
        nlohmann::json tag;
        nlohmann::json light;
        nlohmann::json meshRenderer;
        nlohmann::json rigidbody;
        nlohmann::json collider;
        nlohmann::json hierarchy;
    };

    std::vector<EntityData> entitiesData;
    std::unordered_map<EntityId, EntityId> idMap;

    for (const auto& entityJson : j["entities"]) 
    {
        EntityId oldId = entityJson["id"].get<EntityId>();
        EntityId newId = world.createEntity();
        idMap[oldId] = newId;

        EntityData data;
        data.oldId = oldId;
        if (entityJson.contains("transform")) data.transform = entityJson["transform"];
        if (entityJson.contains("tag")) data.tag = entityJson["tag"];
        if (entityJson.contains("light")) data.light = entityJson["light"];
        if (entityJson.contains("mesh_renderer")) data.meshRenderer = entityJson["mesh_renderer"];
        if (entityJson.contains("rigidbody")) data.rigidbody = entityJson["rigidbody"];
        if (entityJson.contains("collider")) data.collider = entityJson["collider"];
        if (entityJson.contains("hierarchy")) data.hierarchy = entityJson["hierarchy"];

        entitiesData.push_back(data);
    }

    for (const auto& data : entitiesData) 
    {
        EntityId newId = idMap[data.oldId];

        if (data.transform.contains("position")) 
        {
            Transform* transform = world.getComponent<Transform>(newId);
            if (!transform) {
                world.addComponent<Transform>(newId, Transform{});
                transform = world.getComponent<Transform>(newId);
            }
            deserializeTransform(*transform, data.transform);
        }

        if (data.tag.contains("name")) 
        {
            Tag* tag = world.getComponent<Tag>(newId);
            if (!tag) 
            {
                world.addComponent<Tag>(newId, Tag{});
                tag = world.getComponent<Tag>(newId);
            }
            deserializeTag(*tag, data.tag);
        }

        if (data.light.contains("type")) 
        {
            Light* light = world.getComponent<Light>(newId);
            if (!light) 
            {
                world.addComponent<Light>(newId, Light{});
                light = world.getComponent<Light>(newId);
            }
            deserializeLight(*light, data.light);
        }

        if (data.meshRenderer.contains("visible")) 
        {
            MeshRenderer* renderer = world.getComponent<MeshRenderer>(newId);
            if (!renderer) 
            {
                world.addComponent<MeshRenderer>(newId, MeshRenderer{});
                renderer = world.getComponent<MeshRenderer>(newId);
            }
            deserializeMeshRenderer(renderAdapter, *renderer, data.meshRenderer);
        }

        if (data.rigidbody.contains("mass")) 
        {
            Rigidbody* rb = world.getComponent<Rigidbody>(newId);
            if (!rb) 
            {
                world.addComponent<Rigidbody>(newId, Rigidbody{});
                rb = world.getComponent<Rigidbody>(newId);
            }
            deserializeRigidbody(*rb, data.rigidbody);
        }

        if (data.collider.contains("type")) 
        {
            Collider* collider = world.getComponent<Collider>(newId);
            if (!collider) 
            {
                world.addComponent<Collider>(newId, Collider{});
                collider = world.getComponent<Collider>(newId);
            }
            deserializeCollider(*collider, data.collider);
        }

        if (data.hierarchy.contains("parent")) 
        {
            Hierarchy* hierarchy = world.getComponent<Hierarchy>(newId);
            if (!hierarchy) 
            {
                world.addComponent<Hierarchy>(newId, Hierarchy{});
                hierarchy = world.getComponent<Hierarchy>(newId);
            }
            deserializeHierarchy(*hierarchy, data.hierarchy);
        }
    }

    for (const auto& data : entitiesData) 
    {
        if (data.hierarchy.contains("parent")) 
        {
            EntityId newId = idMap[data.oldId];
            Hierarchy* hierarchy = world.getComponent<Hierarchy>(newId);
            if (hierarchy && hierarchy->parent != -1) 
            {
                EntityId oldParentId = hierarchy->parent;
                EntityId newParentId = idMap[oldParentId];
                hierarchy->parent = newParentId;

                if (world.hasComponent<Hierarchy>(newParentId)) 
                {
                    Hierarchy* parentHierarchy = world.getComponent<Hierarchy>(newParentId);
                    parentHierarchy->children.push_back(newId);
                }
            }
        }
    }

    LOG_INFO("Scene loaded, entities: %d", world.getComponentPool<Transform>().getAll().size());
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
            auto meshResource = RESOURCE_MANAGER.load<Mesh>(meshPath);
            if (meshResource && meshResource->isValid()) 
            {
                renderer.mesh = meshResource->get();
                LOG_INFO("Loaded mesh from file: " + meshPath);
            }
            else 
            {
                LOG_WARNING("Failed to load mesh: " + meshPath);
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
            specularColor = glm::vec4(
                mat["ambient_color"][0], mat["ambient_color"][1],
                mat["ambient_color"][2], mat["ambient_color"][3]
            );
        }
        if (mat.contains("shininess")) 
        {
            shininess = mat["shininess"].get<float>();
        }

        Shader* vertShader = nullptr;
        Shader* fragShader = nullptr;
        if (mat.contains("vertex_shader_path"))
        {
            vertShader = RESOURCE_MANAGER.load<Shader>(mat["vertex_shader_path"])->get();
        }
        if (mat.contains("fragment_shader_path"))
        {
            fragShader = RESOURCE_MANAGER.load<Shader>(mat["fragment_shader_path"])->get();
        }
        

        Material* material = new Material{
            diffuseColor, vertShader, fragShader, renderAdapter
        };

        if (mat.contains("has_texture") && mat["has_texture"].get<bool>()) 
        {
            if (mat.contains("diffuse_texture_path")) 
            {
                std::string texturePath = mat["diffuse_texture_path"].get<std::string>();
                auto textureResource = RESOURCE_MANAGER.load<Texture>(texturePath);
                if (textureResource && textureResource->isValid()) 
                {
                    material->diffuseTexture = textureResource->get();
                    material->hasTexture = true;
                    LOG_INFO("Loaded texture: " + texturePath);
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
