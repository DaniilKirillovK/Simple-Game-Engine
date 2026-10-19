#include "Scripting/ScriptEngine.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace py = pybind11;
namespace fs = std::filesystem;

namespace
{
	constexpr float FILE_POLL_INTERVAL = 0.5f;

	fs::path getExecutableDir()
	{
		wchar_t buffer[MAX_PATH];
		DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		return fs::path(std::wstring(buffer, length)).parent_path();
	}

	bool appendSearchPath(PyConfig& config, const fs::path& path)
	{
		PyStatus status = PyWideStringList_Append(&config.module_search_paths, path.wstring().c_str());
		return !PyStatus_Exception(status);
	}
}

bool ScriptEngine::initialize()
{
	if (m_initialized) return true;

	LOG_INFO("Initializing script engine...");

	const fs::path exeDir = getExecutableDir();
	const fs::path pythonHome = exeDir / "python";
	const fs::path stdlibZip = pythonHome / "python312.zip";

	if (!fs::exists(stdlibZip))
	{
		LOG_ERROR("Python runtime not found: " + stdlibZip.string() + ". Scripting disabled");
		return false;
	}

#ifdef ENGINE_SCRIPTS_SOURCE_DIR
	const fs::path sourceScriptsDir = ENGINE_SCRIPTS_SOURCE_DIR;
	m_scriptsDir = fs::exists(sourceScriptsDir) ? sourceScriptsDir : exeDir / "scripts";
#else
	m_scriptsDir = exeDir / "scripts";
#endif

	PyConfig config;
	PyConfig_InitIsolatedConfig(&config);
	config.write_bytecode = 0;

	PyStatus status = PyConfig_SetString(&config, &config.home, pythonHome.wstring().c_str());
	config.module_search_paths_set = 1;
	if (PyStatus_Exception(status)
		|| !appendSearchPath(config, stdlibZip)
		|| !appendSearchPath(config, pythonHome)
		|| !appendSearchPath(config, m_scriptsDir))
	{
		PyConfig_Clear(&config);
		LOG_ERROR("Failed to configure Python interpreter");
		return false;
	}

	try
	{
		py::initialize_interpreter(&config, 0, nullptr, false);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Failed to start Python interpreter: " + std::string(e.what()));
		return false;
	}

	m_initialized = true;

	std::string version = py::str(py::module_::import("sys").attr("version"));
	LOG_INFO("Python " + version + " initialized");
	LOG_INFO("Scripts directory: " + m_scriptsDir.string());
	return true;
}

void ScriptEngine::shutdown()
{
	if (!m_initialized) return;

	LOG_INFO("Shutting down script engine");

	m_activeSystem = nullptr;
	m_modules.clear();

	py::finalize_interpreter();
	m_initialized = false;
}

py::object ScriptEngine::loadBehaviourClass(const std::string& moduleName, const std::string& className)
{
	if (!m_initialized) return py::none();

	ModuleEntry& entry = m_modules[moduleName];
	if (!entry.module && !entry.failed)
	{
		importModule(moduleName, entry);
	}

	if (entry.failed || !entry.module) return py::none();

	if (!py::hasattr(entry.module, className.c_str()))
	{
		LOG_ERROR("Script '" + moduleName + "' has no class '" + className + "'");
		return py::none();
	}

	return entry.module.attr(className.c_str());
}

void ScriptEngine::pollFileChanges(float deltaTime, bool allowReload)
{
	if (!m_initialized) return;

	m_pollTimer += deltaTime;
	if (m_pollTimer < FILE_POLL_INTERVAL) return;
	m_pollTimer = 0.0f;

	for (auto& [moduleName, entry] : m_modules)
	{
		std::error_code ec;
		auto writeTime = fs::last_write_time(modulePath(moduleName), ec);
		if (ec || writeTime == entry.loadedTime) continue;

		if (!allowReload)
		{
			if (!entry.reloadPendingLogged)
			{
				LOG_INFO("Script '" + moduleName + "' changed, it will be reloaded after Stop");
				entry.reloadPendingLogged = true;
			}
			continue;
		}

		if (importModule(moduleName, entry))
		{
			LOG_INFO("Hot reloaded script: " + moduleName);
		}
	}
}

void ScriptEngine::logPythonError(const std::string& context, const std::exception& e)
{
	LOG_ERROR("[Python] " + context + ":\n" + e.what());
}

bool ScriptEngine::importModule(const std::string& moduleName, ModuleEntry& entry)
{
	std::error_code ec;
	entry.loadedTime = fs::last_write_time(modulePath(moduleName), ec);
	entry.reloadPendingLogged = false;

	try
	{
		py::module_ importlib = py::module_::import("importlib");
		importlib.attr("invalidate_caches")();

		if (entry.module)
		{
			entry.module = importlib.attr("reload")(entry.module);
		}
		else
		{
			entry.module = py::module_::import(moduleName.c_str());
		}

		entry.failed = false;
		return true;
	}
	catch (const py::error_already_set& e)
	{
		logPythonError("Failed to load script '" + moduleName + "'", e);
		entry.failed = true;
		return false;
	}
}

fs::path ScriptEngine::modulePath(const std::string& moduleName) const
{
	return m_scriptsDir / (moduleName + ".py");
}
