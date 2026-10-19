#pragma once
#include <pybind11/embed.h>
#include <filesystem>
#include <string>
#include <unordered_map>

class ScriptSystem;

class ScriptEngine
{
public:
	static ScriptEngine& getInstance()
	{
		static ScriptEngine instance;
		return instance;
	}

	ScriptEngine(const ScriptEngine&) = delete;
	ScriptEngine& operator=(const ScriptEngine&) = delete;

	bool initialize();
	void shutdown();
	bool isInitialized() const { return m_initialized; }

	pybind11::object loadBehaviourClass(const std::string& moduleName, const std::string& className);

	void pollFileChanges(float deltaTime, bool allowReload);

	void setActiveSystem(ScriptSystem* system) { m_activeSystem = system; }
	ScriptSystem* getActiveSystem() const { return m_activeSystem; }

	const std::filesystem::path& getScriptsDir() const { return m_scriptsDir; }

	static void logPythonError(const std::string& context, const std::exception& e);

private:
	ScriptEngine() = default;

	struct ModuleEntry
	{
		pybind11::object module;
		std::filesystem::file_time_type loadedTime{};
		bool failed = false;
		bool reloadPendingLogged = false;
	};

	bool importModule(const std::string& moduleName, ModuleEntry& entry);
	std::filesystem::path modulePath(const std::string& moduleName) const;

	bool m_initialized = false;
	std::filesystem::path m_scriptsDir;
	std::unordered_map<std::string, ModuleEntry> m_modules;
	ScriptSystem* m_activeSystem = nullptr;
	float m_pollTimer = 0.0f;
};
