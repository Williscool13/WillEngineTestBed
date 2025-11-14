//
// Created by William on 2025-11-14.
//

#ifndef WILLENGINETESTBED_LOGGER_HELPERS_H
#define WILLENGINETESTBED_LOGGER_HELPERS_H

#include "logger.h"

#define LOG_TRACE(...) Logger::Get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::Get()->debug(__VA_ARGS__)
#define LOG_INFO(...)  Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...)  Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::Get()->critical(__VA_ARGS__)

#endif //WILLENGINETESTBED_LOGGER_HELPERS_H