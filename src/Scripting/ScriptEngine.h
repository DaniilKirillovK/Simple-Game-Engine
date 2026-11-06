#pragma once
#include <pybind11/embed.h>
#include "JobSystem/JobSystem.h"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

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

	pybind11::object loadBehaviourClass(const std::string& moduleName, const std::string& className,
		bool allowLastGood = false);

	std::vector<std::string> pollFileChanges(float deltaTime);

	const std::string* getModuleError(const std::string& moduleName) const;

	bool isGilRequested() const { return m_watch.gilRequested.load(); }

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
		std::string error;
	};

	struct ScriptCheck
	{
		std::string moduleName;
		std::filesystem::path path;
		std::filesystem::file_time_type writeTime{};
		std::string source;
		pybind11::object code;
		std::string error;
	};

	struct WatchTask
	{
		std::filesystem::path scriptsDir;
		std::unordered_map<std::string, std::filesystem::file_time_type> known;
		std::vector<ScriptCheck> results;
		std::atomic<bool> gilRequested{ false };
		JobCounter counter;
	};

	static void watchJob(void* data, uint32_t index);
	static void compileCheck(ScriptCheck& check);

	void startWatchJob();
	void finishWatchJob();
	std::vector<std::string> applyWatchResults();
	bool importModule(const std::string& moduleName, ModuleEntry& entry,
		const pybind11::object& compiled = pybind11::object());
	bool dependsOn(const ModuleEntry& entry, const std::string& moduleName) const;
	std::filesystem::path modulePath(const std::string& moduleName) const;

	bool m_initialized = false;
	std::filesystem::path m_scriptsDir;
	std::unordered_map<std::string, ModuleEntry> m_modules;
	ScriptSystem* m_activeSystem = nullptr;
	float m_pollTimer = 0.0f;
	WatchTask m_watch;
	bool m_watchInFlight = false;
};
