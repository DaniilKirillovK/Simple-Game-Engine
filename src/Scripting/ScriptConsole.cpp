#include "Scripting/ScriptConsole.h"
#include "Scripting/ScriptEngine.h"
#include "Systems/ScriptSystem.h"
#include "World.h"

#include "tracy/Tracy.hpp"

namespace py = pybind11;

constexpr size_t MAX_LINES = 2000;
constexpr size_t MAX_HISTORY = 200;

class StreamCapture
{
public:
	StreamCapture()
	{
		py::module_ io = py::module_::import("io");
		m_sys = py::module_::import("sys");

		m_oldIn = m_sys.attr("stdin");
		m_oldOut = m_sys.attr("stdout");
		m_oldErr = m_sys.attr("stderr");

		m_out = io.attr("StringIO")();
		m_err = io.attr("StringIO")();

		m_sys.attr("stdin") = io.attr("StringIO")();
		m_sys.attr("stdout") = m_out;
		m_sys.attr("stderr") = m_err;
	}

	~StreamCapture()
	{
		try
		{
			m_sys.attr("stdin") = m_oldIn;
			m_sys.attr("stdout") = m_oldOut;
			m_sys.attr("stderr") = m_oldErr;
		}
		catch (const std::exception&)
		{
		}
	}

	std::string out() const { return py::cast<std::string>(m_out.attr("getvalue")()); }
	std::string err() const { return py::cast<std::string>(m_err.attr("getvalue")()); }

private:
	py::module_ m_sys;
	py::object m_oldIn, m_oldOut, m_oldErr;
	py::object m_out, m_err;
};

ScriptConsole::~ScriptConsole()
{
	if (ScriptEngine::getInstance().isInitialized()) return;

	m_console.release();
	m_namespace.release();
}

void ScriptConsole::submit(const std::string& source, EntityId selectedEntity)
{
	ZoneScopedN("ScriptConsole::submit");

	addText(LineType::Input, (m_needsMore ? "... " : ">>> ") + source);

	if (!source.empty() && (m_history.empty() || m_history.back() != source))
	{
		m_history.push_back(source);
		if (m_history.size() > MAX_HISTORY)
		{
			m_history.erase(m_history.begin());
		}
	}

	if (!ensureConsole()) return;

	std::string out;
	std::string err;

	try
	{
		updateSelection(selectedEntity);

		StreamCapture capture;
		try
		{
			m_needsMore = py::cast<bool>(m_console.attr("push")(source));
		}
		catch (const py::error_already_set& e)
		{
			m_console.attr("resetbuffer")();
			m_needsMore = false;

			err = e.matches(PyExc_SystemExit)
				? "exit() is disabled in the engine console, use Reset to start over\n"
				: std::string(e.what()) + "\n";
		}

		out = capture.out();
		err = capture.err() + err;
	}
	catch (const std::exception& e)
	{
		m_needsMore = false;
		err += std::string(e.what()) + "\n";
	}

	addText(LineType::Output, out);
	addText(LineType::Error, err);
}

void ScriptConsole::reset()
{
	m_needsMore = false;

	if (!ScriptEngine::getInstance().isInitialized()) return;

	m_console = py::object();
	m_namespace = py::object();
	addText(LineType::Info, "Namespace reset");
}

bool ScriptConsole::ensureConsole()
{
	if (!ScriptEngine::getInstance().isInitialized())
	{
		addText(LineType::Error, "Python is not initialized, see the engine log");
		return false;
	}

	if (m_console) return true;

	try
	{
		py::dict globals;
		globals["__name__"] = "__console__";
		globals["__doc__"] = py::none();
		globals["engine"] = py::module_::import("engine");
		globals["sel"] = py::none();

		m_console = py::module_::import("code").attr("InteractiveConsole")(globals, "<console>");
		m_namespace = globals;
		return true;
	}
	catch (const std::exception& e)
	{
		addText(LineType::Error, std::string("Cannot start the console: ") + e.what());
		return false;
	}
}

void ScriptConsole::updateSelection(EntityId selectedEntity)
{
	py::object selected = py::none();

	ScriptSystem* system = ScriptEngine::getInstance().getActiveSystem();
	if (system && system->getWorld().isValidEntity(selectedEntity))
	{
		py::module_ engine = py::module_::import("engine");
		selected = engine.attr("resolve_entity")(selectedEntity, system->getSessionId());
	}

	m_namespace.attr("__setitem__")("sel", selected);
}

void ScriptConsole::addText(LineType type, const std::string& text)
{
	size_t start = 0;
	while (start < text.size())
	{
		size_t end = text.find('\n', start);
		if (end == std::string::npos) end = text.size();

		m_lines.push_back({ type, text.substr(start, end - start) });
		start = end + 1;
	}

	if (m_lines.size() > MAX_LINES)
	{
		m_lines.erase(m_lines.begin(), m_lines.begin() + (m_lines.size() - MAX_LINES));
	}
}
