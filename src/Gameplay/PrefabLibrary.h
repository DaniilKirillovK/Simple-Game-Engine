#pragma once
#include "Entity.h"
#include "Components/MeshRenderer.h"
#include "Components/Script.h"
#include "Utils/json.hpp"

#include <glm/glm.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class World;
class IRenderAdapter;

// Data-driven entity templates: <prefabs dir>/<name>.prefab.json = { "version": 1, "entity": {...} }.
// "entity" uses the scene entity format, so SceneSerializer parses both.
// A template is re-read when its file changes: balance edits apply to the next spawn,
// already spawned instances keep their values.
class PrefabLibrary
{
public:
	explicit PrefabLibrary(IRenderAdapter& renderAdapter);

	// Position replaces the template's one; fieldOverrides are merged over the template's script fields.
	// Returns INVALID_ENTITY and logs the reason on failure.
	EntityId instantiate(World& world, const std::string& name, const glm::vec3& position,
		const nlohmann::json& fieldOverrides = nlohmann::json::object());

	std::vector<std::string> list() const;

	// Template's script component (module, class, stored fields), nullopt if the prefab has none
	std::optional<Script> getScript(const std::string& name);

	// Rewrites only entity.script.fields in the file, the rest of the template is kept as is
	bool saveScriptFields(const std::string& name, const nlohmann::json& fields);

	// Writes the entity as a template and links the entity to it. Hierarchy and entity
	// references are dropped: they point into one particular scene.
	bool saveFromEntity(World& world, EntityId entity, const std::string& name);

	const std::filesystem::path& getDirectory() const { return m_directory; }

	static bool isValidName(const std::string& name);

private:
	struct Prefab
	{
		nlohmann::json entity;                // components except mesh_renderer
		std::optional<MeshRenderer> renderer; // mesh and material shared by all instances
		std::filesystem::file_time_type checkedTime{};
		bool valid = false;
	};

	const Prefab* get(const std::string& name);
	bool load(const std::filesystem::path& path, Prefab& prefab);
	std::filesystem::path pathOf(const std::string& name) const;

	IRenderAdapter& m_renderAdapter;
	std::filesystem::path m_directory;
	std::unordered_map<std::string, Prefab> m_prefabs;
};
