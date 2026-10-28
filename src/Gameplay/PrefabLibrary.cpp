#include "Gameplay/PrefabLibrary.h"
#include "Utils/Serialization/SceneSerializer.h"
#include "Scripting/EntityReferenceData.h"
#include "World.h"
#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#include "tracy/Tracy.hpp"

namespace fs = std::filesystem;
using Json = nlohmann::json;


constexpr const char* PREFAB_EXTENSION = ".prefab.json";
constexpr int PREFAB_VERSION = 1;

const char* const KNOWN_COMPONENTS[] =
{
	"transform", "tag", "mesh_renderer", "rigidbody", "collider", "light", "camera", "script"
};


PrefabLibrary::PrefabLibrary(IRenderAdapter& renderAdapter)
	: m_renderAdapter(renderAdapter)
{
#ifdef ENGINE_PREFABS_SOURCE_DIR
	// Dev build: read templates from the source tree, so edits there are picked up live
	const fs::path sourceDir = ENGINE_PREFABS_SOURCE_DIR;
	m_directory = fs::exists(sourceDir) ? sourceDir : fs::path("assets") / "prefabs";
#else
	m_directory = fs::path("assets") / "prefabs";
#endif
	LOG_INFO("Prefabs directory: " + m_directory.string());
}

bool PrefabLibrary::isValidName(const std::string& name)
{
	// Names are file stems; no separators or dots, so a script cannot reach outside the folder
	return !name.empty() && std::all_of(name.begin(), name.end(), [](char c)
		{
			return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
		});
}

fs::path PrefabLibrary::pathOf(const std::string& name) const
{
	return m_directory / (name + PREFAB_EXTENSION);
}

const PrefabLibrary::Prefab* PrefabLibrary::get(const std::string& name)
{
	if (!isValidName(name))
	{
		LOG_ERROR("Invalid prefab name '" + name + "': use letters, digits, '_' and '-'");
		return nullptr;
	}

	const fs::path path = pathOf(name);

	std::error_code ec;
	const auto writeTime = fs::last_write_time(path, ec);
	if (ec)
	{
		LOG_ERROR("Prefab '" + name + "' not found: " + path.string());
		return nullptr;
	}

	Prefab& cached = m_prefabs[name];
	if (cached.checkedTime == writeTime)
	{
		return cached.valid ? &cached : nullptr;
	}

	// Changed or never loaded. On error keep the previous valid version, the game goes on
	cached.checkedTime = writeTime;

	Prefab fresh;
	if (load(path, fresh))
	{
		const bool reloaded = cached.valid;
		fresh.checkedTime = writeTime;
		cached = std::move(fresh);
		LOG_INFO(std::string(reloaded ? "Prefab reloaded: " : "Prefab loaded: ") + name);
	}
	else if (cached.valid)
	{
		LOG_WARNING("Prefab '" + name + "' keeps its previous version until the file is fixed");
	}

	return cached.valid ? &cached : nullptr;
}

bool PrefabLibrary::load(const fs::path& path, Prefab& prefab)
{
	ZoneScopedN("PrefabLibrary::load");

	const std::string file = path.string();

	Json root;
	try
	{
		std::ifstream stream(path);
		if (!stream.is_open())
		{
			LOG_ERROR("Cannot open prefab: " + file);
			return false;
		}
		stream >> root;
	}
	catch (const Json::parse_error& e)
	{
		// e.what() carries line and column
		LOG_ERROR("Prefab " + file + ": " + e.what());
		return false;
	}

	if (!root.is_object() || !root.contains("entity") || !root["entity"].is_object())
	{
		LOG_ERROR("Prefab " + file + ": expected { \"version\": 1, \"entity\": { ... } }");
		return false;
	}

	if (root.value("version", PREFAB_VERSION) != PREFAB_VERSION)
	{
		LOG_ERROR("Prefab " + file + ": unsupported version " + root["version"].dump());
		return false;
	}

	Json entity = root["entity"];
	entity.erase("id");
	entity.erase("hierarchy");
	entity.erase("prefab");

	for (const auto& item : entity.items())
	{
		const bool known_comp = std::any_of(std::begin(KNOWN_COMPONENTS), std::end(KNOWN_COMPONENTS),
			[&](const char* component) { return item.key() == component; });
		if (!known_comp)
		{
			LOG_WARNING("Prefab " + file + ": unknown component '" + item.key() + "' ignored");
		}
	}

	if (entity.contains("script"))
	{
		const Json& entity_script = entity["script"];
		if (!entity_script.is_object() || entity_script.value("module", "").empty() || entity_script.value("class", "").empty())
		{
			LOG_ERROR("Prefab " + file + ": script needs non-empty \"module\" and \"class\"");
			return false;
		}
		if (entity_script.contains("fields") && !entity_script["fields"].is_object())
		{
			LOG_ERROR("Prefab " + file + ": script \"fields\" must be an object");
			return false;
		}
	}

	try
	{
		// Built once per template version: every instance shares one mesh and one shader program
		if (entity.contains("mesh_renderer"))
		{
			MeshRenderer renderer;
			SceneSerializer::deserializeMeshRenderer(m_renderAdapter, renderer, entity["mesh_renderer"]);
			prefab.renderer = renderer;
			entity.erase("mesh_renderer");
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Prefab " + file + ": invalid mesh_renderer: " + e.what());
		return false;
	}

	prefab.entity = std::move(entity);
	prefab.valid = true;
	return true;
}

EntityId PrefabLibrary::instantiate(World& world, const std::string& name, const glm::vec3& position,
	const Json& fieldOverrides)
{
	ZoneScopedN("PrefabLibrary::instantiate");

	const Prefab* prefab = get(name);
	if (!prefab) return INVALID_ENTITY;

	const EntityId entity = world.createEntity();

	try
	{
		// Empty id map: entity references stored in a template resolve to null
		SceneSerializer::deserializeEntity(m_renderAdapter, world, entity, prefab->entity, {});
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Prefab '" + name + "' has invalid data: " + e.what());
		world.destroyEntity(entity);
		return INVALID_ENTITY;
	}

	if (prefab->renderer)
	{
		world.getComponentPool<MeshRenderer>().addComponent(entity, *prefab->renderer);
	}

	Transform* transform = world.getComponent<Transform>(entity);
	if (!transform)
	{
		world.getComponentPool<Transform>().addComponent(entity, Transform{});
		transform = world.getComponent<Transform>(entity);
	}
	transform->position = position;
	transform->markDirty();

	if (!world.getComponent<Tag>(entity))
	{
		world.getComponentPool<Tag>().addComponent(entity, Tag{ name });
	}

	world.getComponentPool<PrefabInstance>().addComponent(entity, PrefabInstance{ name });

	if (fieldOverrides.is_object() && !fieldOverrides.empty())
	{
		if (Script* script = world.getComponent<Script>(entity))
		{
			// Values are validated against the behaviour's schema when the behaviour is created
			script->fields.update(fieldOverrides);
		}
		else
		{
			LOG_WARNING("Prefab '" + name + "' has no script, field overrides ignored");
		}
	}

	return entity;
}

std::vector<std::string> PrefabLibrary::list() const
{
	std::vector<std::string> names;

	std::error_code ec;
	for (const auto& item : fs::directory_iterator(m_directory, ec))
	{
		const std::string fileName = item.path().filename().string();
		const std::string extension = PREFAB_EXTENSION;
		if (fileName.size() > extension.size() &&
			fileName.compare(fileName.size() - extension.size(), extension.size(), extension) == 0)
		{
			names.push_back(fileName.substr(0, fileName.size() - extension.size()));
		}
	}

	std::sort(names.begin(), names.end());
	return names;
}

bool PrefabLibrary::saveFromEntity(World& world, EntityId entity, const std::string& name)
{
	if (!isValidName(name))
	{
		LOG_ERROR("Invalid prefab name '" + name + "': use letters, digits, '_' and '-'");
		return false;
	}
	if (!world.isValidEntity(entity))
	{
		LOG_ERROR("Cannot save prefab '" + name + "': no entity selected");
		return false;
	}

	Json entityJson = SceneSerializer::serializeEntity(world, entity);
	entityJson.erase("hierarchy");
	entityJson.erase("prefab");

	if (entityJson.contains("transform"))
	{
		// Spawn position comes from the caller
		entityJson["transform"]["position"] = { 0.0f, 0.0f, 0.0f };
	}
	if (entityJson.contains("script"))
	{
		EntityReferenceData::remapFields(entityJson["script"]["fields"], {});
	}

	Json root;
	root["version"] = PREFAB_VERSION;
	root["entity"] = std::move(entityJson);

	std::error_code ec;
	fs::create_directories(m_directory, ec);

	const fs::path path = pathOf(name);
	std::ofstream stream(path);
	if (!stream.is_open())
	{
		LOG_ERROR("Cannot write prefab: " + path.string());
		return false;
	}
	stream << root.dump(4);
	stream.close();

	// Force a re-read on the next spawn even if the timestamp resolution hides the change
	m_prefabs.erase(name);

	world.getComponentPool<PrefabInstance>().addComponent(entity, PrefabInstance{ name });

	LOG_INFO("Prefab saved: " + path.string());
	return true;
}

std::optional<Script> PrefabLibrary::getScript(const std::string& name)
{
	const Prefab* prefab = get(name);
	if (!prefab || !prefab->entity.contains("script")) return std::nullopt;

	Script script;
	SceneSerializer::deserializeScript(script, prefab->entity["script"]);
	return script;
}

bool PrefabLibrary::saveScriptFields(const std::string& name, const Json& fields)
{
	if (!isValidName(name) || !fields.is_object())
	{
		LOG_ERROR("Cannot save fields of prefab '" + name + "'");
		return false;
	}

	// Edit the raw file, not the cache: the cache has mesh_renderer stripped
	const fs::path path = pathOf(name);
	Json root;
	try
	{
		std::ifstream stream(path);
		if (!stream.is_open())
		{
			LOG_ERROR("Cannot open prefab: " + path.string());
			return false;
		}
		stream >> root;
	}
	catch (const Json::parse_error& e)
	{
		LOG_ERROR("Prefab " + path.string() + ": " + e.what());
		return false;
	}

	if (!root.is_object() || !root.contains("entity") || !root["entity"].is_object()
		|| !root["entity"].contains("script") || !root["entity"]["script"].is_object())
	{
		LOG_ERROR("Prefab " + path.string() + " has no script to save fields into");
		return false;
	}

	Json storedFields = fields;
	EntityReferenceData::remapFields(storedFields, {});
	root["entity"]["script"]["fields"] = std::move(storedFields);

	std::ofstream stream(path);
	if (!stream.is_open())
	{
		LOG_ERROR("Cannot write prefab: " + path.string());
		return false;
	}
	stream << root.dump(4);
	stream.close();

	m_prefabs.erase(name);

	LOG_INFO("Prefab fields saved: " + path.string());
	return true;
}
