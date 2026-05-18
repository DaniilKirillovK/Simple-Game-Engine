#pragma once
#include <string>
#include <fstream>
#include <chrono>
#include <functional>

constexpr int LOG_DETAILS_LEVEL = 3;

#define LOG_INFO(msg) Logger::log(LogLevel::Info, msg)
#define LOG_WARNING(msg) Logger::log(LogLevel::Warning, msg)
#define LOG_ERROR(msg) Logger::log(LogLevel::Error, msg)
#define LOG_INPUT(msg) Logger::log(LogLevel::Info, msg, 4);
#define LOG_RESOURCEMANAGER(msg) Logger::log(LogLevel::ResourceManager, msg, 3);
#define LOG_RESOURCEMANAGER_ERROR(msg) Logger::log(LogLevel::ResourceManagerError, msg, 3);

enum class LogLevel { Info, Warning, Error, ResourceManager, ResourceManagerError };

struct LogEntry
{
    std::string message;
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
};

static std::function<void(LogLevel, const std::string&)> g_logCallback = nullptr;

class Logger 
{
public:
    static Logger& instance();
    static void log(LogLevel level, const std::string& message, int detailsLevel = 1);
    static void setLogFile(const std::string& filename);

    void setLogCallback(std::function<void(LogLevel, const std::string&)> callback);

private:
    Logger() = default;
    static std::ofstream fileStream;
};