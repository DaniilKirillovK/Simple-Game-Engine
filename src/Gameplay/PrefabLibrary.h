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

class PrefabLibrary
{
public:
	explicit PrefabLibrary(IRenderAdapter& renderAdapter);

	EntityId instantiate(World& world, const std::string& name, const glm::vec3& position,
		const nlohmann::json& fieldOverrides = nlohmann::json::object());

	std::vector<std::string> list() const;

	std::optional<Script> getScript(const std::string& name);

	bool saveScriptFields(const std::string& name, const nlohmann::json& fields);

	bool saveFromEntity(World& world, EntityId entity, const std::string& name);

	const std::filesystem::path& getDirectory() const { return m_directory; }

	static bool isValidName(const std::string& name);

private:
	struct Prefab
	{
		nlohmann::json entity;
		std::optional<MeshRenderer> renderer;
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
