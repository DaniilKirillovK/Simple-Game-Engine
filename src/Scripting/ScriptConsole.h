#pragma once

#include "Entity.h"

#include <pybind11/pybind11.h>

#include <string>
#include <vector>

class ScriptConsole
{
public:
	enum class LineType
	{
		Input,
		Output,
		Error,
		Info,
	};

	struct Line
	{
		LineType type;
		std::string text;
	};

	ScriptConsole() = default;
	~ScriptConsole();

	ScriptConsole(const ScriptConsole&) = delete;
	ScriptConsole& operator=(const ScriptConsole&) = delete;

	void submit(const std::string& source, EntityId selectedEntity);

	void reset();
	void clearOutput() { m_lines.clear(); }

	bool needsMoreInput() const { return m_needsMore; }

	const std::vector<Line>& lines() const { return m_lines; }
	const std::vector<std::string>& history() const { return m_history; }

private:
	bool ensureConsole();
	void updateSelection(EntityId selectedEntity);
	void addText(LineType type, const std::string& text);

	pybind11::object m_console;
	pybind11::object m_namespace;

	bool m_needsMore = false;
	std::vector<Line> m_lines;
	std::vector<std::string> m_history;
};
