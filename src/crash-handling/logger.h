//
// Created by William on 2025-09-26.
//

#ifndef WILLENGINETESTBED_LOGGER_H
#define WILLENGINETESTBED_LOGGER_H

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

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

#endif //WILLENGINETESTBED_LOGGER_H
