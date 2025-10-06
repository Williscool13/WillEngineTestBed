//
// Created by William on 2025-09-26.
//

#include "../../crash-handling/logger.h"

#include <filesystem>

#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> Logger::logger = nullptr;
std::string Logger::logPath;
bool Logger::bInitialized = false;

void Logger::Initialize(const std::string& _logPath)
{
    if (bInitialized) return;

    logPath = _logPath;

    const std::filesystem::path path(_logPath);
    std::filesystem::create_directories(path.parent_path());

    try {
        const auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(_logPath, true);
        const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        consoleSink->set_pattern("[%^%l%$] %v");

        std::vector<spdlog::sink_ptr> sinks {fileSink, consoleSink};
        logger = std::make_shared<spdlog::logger>("engine", sinks.begin(), sinks.end());

#ifdef NDEBUG
        logger->set_level(spdlog::level::info);
#else
        logger->set_level(spdlog::level::debug);
#endif
        logger->flush_on(spdlog::level::warn);
        spdlog::register_logger(logger);

        bInitialized = true;

        LOG_INFO("Initialized logger ({})", _logPath);
    }
    catch (const std::exception& ex) {
        fmt::print("Initialized logger ({})", ex.what());
    }
}

void Logger::Shutdown()
{
    if (logger) {
        logger->flush();
        spdlog::drop_all();
        logger = nullptr;
    }
    bInitialized = false;
}

void Logger::Flush()
{
    if (logger) {
        logger->flush();
    }
}