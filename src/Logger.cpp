#include "Logger.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>
#include <thread>

std::ofstream Logger::fileStream;
std::mutex mtx;

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const std::string& message, int detailsLevel) 
{
    if (LOG_DETAILS_LEVEL >= detailsLevel)
    {
        mtx.lock();

        std::string levelStr;
        switch (level)
        {
        case LogLevel::Info:
            levelStr = "INFO";
            break;
        case LogLevel::Warning:
            levelStr = "WARNING";
            break;
        case LogLevel::Error:
            levelStr = "ERROR";
            break;
        }
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char timeStr[26];
        ctime_s(timeStr, sizeof(timeStr), &t);
        timeStr[24] = '\0';

        std::string formatted = "[" + std::string(timeStr) + "] [" + levelStr + "] " + message;
        std::cout << formatted << std::endl;
        if (fileStream.is_open()) 
        {
            fileStream << formatted << std::endl;
        }

        mtx.unlock();
    }
}

void Logger::setLogFile(const std::string& filename)
{
    if (fileStream.is_open()) 
    {
        fileStream.close();
    }

    fileStream.open(filename, std::ios::out | std::ios::trunc);

    if (fileStream.is_open()) 
    {
        LOG_INFO("Log file opened: " + filename);
    }
    else 
    {
        LOG_WARNING("Failed to open log file: " + filename);
    }
}
