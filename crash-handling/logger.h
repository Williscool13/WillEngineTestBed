//
// Created by William on 2025-09-26.
//

#ifndef WILLENGINETESTBED_LOGGER_H
#define WILLENGINETESTBED_LOGGER_H

#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

class Logger
{
public:
    static void Initialize(const std::string& _logPath = "logs/engine.log");

    static void Shutdown();

    // Get the main logger
    static std::shared_ptr<spdlog::logger> Get() { return logger; }

    // Get current log file path for crash copying
    static std::string GetCurrentLogPath() { return logPath; }

    // Force flush logs (useful before crashes)
    static void Flush();

    static bool IsInitialized() { return bInitialized; }

private:
    static std::shared_ptr<spdlog::logger> logger;
    static std::string logPath;
    static bool bInitialized;
};

#define LOG_TRACE(...) Logger::Get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::Get()->debug(__VA_ARGS__)
#define LOG_INFO(...)  Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...)  Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::Get()->critical(__VA_ARGS__)

#endif //WILLENGINETESTBED_LOGGER_H
