#pragma once

class ScriptGilWindow
{
public:
	ScriptGilWindow();
	~ScriptGilWindow();

	ScriptGilWindow(const ScriptGilWindow&) = delete;
	ScriptGilWindow& operator=(const ScriptGilWindow&) = delete;

private:
	void* m_threadState = nullptr;
};
