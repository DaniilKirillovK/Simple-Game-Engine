#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptGilWindow.h"
#include "Logger.h"
#include "tracy/Tracy.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <unordered_set>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace py = pybind11;
namespace fs = std::filesystem;

constexpr float FILE_POLL_INTERVAL = 0.5f;

namespace
{
	fs::path getExecutableDir()
	{
		wchar_t buffer[MAX_PATH];
		DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		return fs::path(std::wstring(buffer, length)).parent_path();
	}

	bool appendSearchPath(PyConfig & config, const fs::path & path)
	{
		PyStatus status = PyWideStringList_Append(&config.module_search_paths, path.wstring().c_str());
		return !PyStatus_Exception(status);
	}
}

static std::string fetchPythonError()
{
	PyObject* error = PyErr_GetRaisedException();
	if (!error) return "Unknown Python error";

	std::string message = Py_TYPE(error)->tp_name;

	PyObject* text = PyObject_Str(error);
	const char* utf8 = text ? PyUnicode_AsUTF8(text) : nullptr;
	if (utf8)
	{
		message += ": ";
		message += utf8;
	}

	Py_XDECREF(text);
	Py_DECREF(error);
	PyErr_Clear();
	return message;
}

static bool isModuleName(const std::string& name)
{
	if (name.empty() || name.rfind("__", 0) == 0) return false;
	if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') return false;

	return std::all_of(name.begin(), name.end(), [](char c)
		{
			return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
		});
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

	finishWatchJob();

	m_activeSystem = nullptr;
	m_modules.clear();

	py::finalize_interpreter();
	m_initialized = false;
}

py::object ScriptEngine::loadBehaviourClass(const std::string& moduleName, const std::string& className,
	bool allowLastGood)
{
	if (!m_initialized) return py::none();

	ModuleEntry& entry = m_modules[moduleName];
	if (!entry.module && !entry.failed)
	{
		importModule(moduleName, entry);
	}

	if (!entry.module || (entry.failed && !allowLastGood)) return py::none();

	if (!py::hasattr(entry.module, className.c_str()))
	{
		LOG_ERROR("Script '" + moduleName + "' has no class '" + className + "'");
		return py::none();
	}

	return entry.module.attr(className.c_str());
}

std::vector<std::string> ScriptEngine::pollFileChanges(float deltaTime)
{
	if (!m_initialized) return {};

	std::vector<std::string> loaded;
	if (m_watchInFlight && m_watch.counter.isDone())
	{
		m_watchInFlight = false;
		loaded = applyWatchResults();
	}

	m_pollTimer += deltaTime;
	if (m_pollTimer >= FILE_POLL_INTERVAL && !m_watchInFlight)
	{
		m_pollTimer = 0.0f;
		startWatchJob();
	}

	return loaded;
}

void ScriptEngine::startWatchJob()
{
	m_watch.scriptsDir = m_scriptsDir;
	m_watch.results.clear();
	m_watch.known.clear();
	for (const auto& [moduleName, entry] : m_modules)
	{
		m_watch.known[moduleName] = entry.loadedTime;
	}

	m_watchInFlight = true;
	JobSystem::get().parallelForAsync(1, 1, &ScriptEngine::watchJob, &m_watch, &m_watch.counter,
		"ScriptEngine::watchJob", Priority::Low);
}

void ScriptEngine::finishWatchJob()
{
	if (!m_watchInFlight) return;

	{
		py::gil_scoped_release release;
		JobSystem::get().wait(&m_watch.counter);
	}

	m_watch.results.clear();
	m_watchInFlight = false;
}

void ScriptEngine::watchJob(void* data, uint32_t)
{
	WatchTask& watch = *static_cast<WatchTask*>(data);

	{
		ZoneScopedN("ScriptEngine::scanFiles");

		std::error_code ec;
		for (const auto& file : fs::directory_iterator(watch.scriptsDir, ec))
		{
			if (!file.is_regular_file(ec) || file.path().extension() != ".py") continue;

			const std::string moduleName = file.path().stem().string();
			if (!isModuleName(moduleName)) continue;

			const fs::file_time_type writeTime = fs::last_write_time(file.path(), ec);
			if (ec) continue;

			auto known = watch.known.find(moduleName);
			if (known != watch.known.end() && known->second == writeTime) continue;

			std::ifstream stream(file.path(), std::ios::binary);
			if (!stream) continue;

			ScriptCheck check;
			check.moduleName = moduleName;
			check.path = file.path();
			check.writeTime = writeTime;
			check.source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
			watch.results.push_back(std::move(check));
		}
	}

	if (watch.results.empty()) return;

	watch.gilRequested.store(true);
	for (ScriptCheck& check : watch.results)
	{
		compileCheck(check);
	}
	watch.gilRequested.store(false);
}

void ScriptEngine::compileCheck(ScriptCheck& check)
{
	const PyGILState_STATE gil = PyGILState_Ensure();

	{
		ZoneScopedN("ScriptEngine::compileCheck");
		ZoneText(check.moduleName.c_str(), check.moduleName.size());

		const std::string path = check.path.u8string();
		PyObject* code = Py_CompileStringExFlags(check.source.c_str(), path.c_str(), Py_file_input, nullptr, -1);

		if (check.source.find('\0') != std::string::npos)
		{
			check.error = "SyntaxError: source code cannot contain null bytes";
			Py_XDECREF(code);
			PyErr_Clear();
		}
		else if (code)
		{
			check.code = py::reinterpret_steal<py::object>(code);
		}
		else
		{
			check.error = fetchPythonError();
		}
	}

	PyGILState_Release(gil);
}

ScriptGilWindow::ScriptGilWindow()
{
	if (ScriptEngine::getInstance().isGilRequested())
	{
		m_threadState = PyEval_SaveThread();
	}
}

ScriptGilWindow::~ScriptGilWindow()
{
	if (m_threadState)
	{
		PyEval_RestoreThread(static_cast<PyThreadState*>(m_threadState));
	}
}

std::vector<std::string> ScriptEngine::applyWatchResults()
{
	std::vector<ScriptCheck> checks = std::move(m_watch.results);
	m_watch.results.clear();
	if (checks.empty()) return {};

	ZoneScopedN("ScriptEngine::applyWatchResults");

	std::vector<std::string> loaded;
	std::vector<std::string> reloaded;
	int firstLoads = 0;
	int firstLoadFailures = 0;

	for (const ScriptCheck& check : checks)
	{
		const std::string& moduleName = check.moduleName;
		ModuleEntry& entry = m_modules[moduleName];
		if (entry.loadedTime == check.writeTime) continue;

		const bool firstLoad = !entry.module && entry.loadedTime == fs::file_time_type{};

		bool ok = false;
		if (check.error.empty())
		{
			ok = importModule(moduleName, entry, check.code);
		}
		else
		{
			LOG_ERROR("[Python] Failed to load script '" + moduleName + "':\n" + check.error);
			entry.failed = true;
			entry.error = check.error;
		}
		entry.loadedTime = check.writeTime;

		if (ok)
		{
			loaded.push_back(moduleName);
		}
		if (ok && !firstLoad)
		{
			reloaded.push_back(moduleName);
		}

		if (firstLoad)
		{
			++firstLoads;
			if (!ok) ++firstLoadFailures;
		}
		else if (ok)
		{
			LOG_INFO("Hot reloaded script: " + moduleName);
		}
	}

	if (firstLoads > 0)
	{
		LOG_INFO("Scripts checked: " + std::to_string(firstLoads - firstLoadFailures) + " loaded, "
			+ std::to_string(firstLoadFailures) + " with errors");
	}

	std::unordered_set<std::string> dependents;
	for (size_t i = 0; i < reloaded.size(); ++i)
	{
		const std::string dependency = reloaded[i];

		std::vector<std::string> users;
		for (const auto& [moduleName, entry] : m_modules)
		{
			if (moduleName != dependency && !dependents.count(moduleName) && dependsOn(entry, dependency))
			{
				users.push_back(moduleName);
			}
		}

		for (const std::string& moduleName : users)
		{
			dependents.insert(moduleName);
			if (importModule(moduleName, m_modules[moduleName]))
			{
				LOG_INFO("Hot reloaded script: " + moduleName + " (uses a changed module)");
				reloaded.push_back(moduleName);
				loaded.push_back(moduleName);
			}
		}
	}

	return loaded;
}

const std::string* ScriptEngine::getModuleError(const std::string& moduleName) const
{
	auto it = m_modules.find(moduleName);
	if (it == m_modules.end() || !it->second.failed || it->second.error.empty()) return nullptr;
	return &it->second.error;
}

void ScriptEngine::logPythonError(const std::string& context, const std::exception& e)
{
	LOG_ERROR("[Python] " + context + ":\n" + e.what());
}

bool ScriptEngine::importModule(const std::string& moduleName, ModuleEntry& entry, const py::object& compiled)
{
	ZoneScopedN("ScriptEngine::importModule");
	ZoneText(moduleName.c_str(), moduleName.size());

	std::error_code ec;
	entry.loadedTime = fs::last_write_time(modulePath(moduleName), ec);

	py::object lastGood;

	try
	{
		py::module_ importlib = py::module_::import("importlib");
		importlib.attr("invalidate_caches")();

		if (entry.module)
		{
			lastGood = entry.module.attr("__dict__").attr("copy")();
			if (compiled)
			{
				py::module_::import("builtins").attr("exec")(compiled, entry.module.attr("__dict__"));
			}
			else
			{
				entry.module = importlib.attr("reload")(entry.module);
			}
		}
		else
		{
			entry.module = py::module_::import(moduleName.c_str());
		}

		entry.failed = false;
		entry.error.clear();
		return true;
	}
	catch (const py::error_already_set& e)
	{
		logPythonError("Failed to load script '" + moduleName + "'", e);
		entry.failed = true;
		entry.error = e.what();

		if (lastGood)
		{
			py::object moduleDict = entry.module.attr("__dict__");
			moduleDict.attr("clear")();
			moduleDict.attr("update")(lastGood);
		}
		return false;
	}
}

bool ScriptEngine::dependsOn(const ModuleEntry& entry, const std::string& moduleName) const
{
	if (!entry.module) return false;

	try
	{
		for (auto item : py::reinterpret_borrow<py::dict>(entry.module.attr("__dict__")))
		{
			py::handle value = item.second;

			if (py::isinstance<py::module_>(value))
			{
				if (py::str(value.attr("__name__")).cast<std::string>() == moduleName) return true;
				continue;
			}

			if (py::hasattr(value, "__module__"))
			{
				py::object owner = value.attr("__module__");
				if (py::isinstance<py::str>(owner) && owner.cast<std::string>() == moduleName) return true;
			}
		}
	}
	catch (const std::exception&)
	{
	}

	return false;
}

fs::path ScriptEngine::modulePath(const std::string& moduleName) const
{
	return m_scriptsDir / (moduleName + ".py");
}
