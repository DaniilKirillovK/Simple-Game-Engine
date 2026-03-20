#pragma once
#include <string>
#include <fstream>
#include <mutex>

constexpr int LOG_DETAILS_LEVEL = 4;

#define LOG_INFO(msg) Logger::log(LogLevel::Info, msg)
#define LOG_WARNING(msg) Logger::log(LogLevel::Warning, msg)
#define LOG_ERROR(msg) Logger::log(LogLevel::Error, msg)

enum class LogLevel { Info, Warning, Error };

class Logger 
{
public:
    static Logger& instance();
    static void log(LogLevel level, const std::string& message, int detailsLevel = 1);
    static void setLogFile(const std::string& filename);

private:
    Logger() = default;
    static std::ofstream fileStream;
};