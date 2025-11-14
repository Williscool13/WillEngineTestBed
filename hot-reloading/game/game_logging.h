//
// Created by William on 2025-11-14.
//

#ifndef WILLENGINETESTBED_ENGINE_LOGGING_H
#define WILLENGINETESTBED_ENGINE_LOGGING_H

#include <fmt/format.h>

#include "hot-reloading/engine/engine_api.h"

namespace HotReloading::Game
{
void LogTrace(const char* msg);
void LogDebug(const char* msg);

template<typename... Args>
void LogInfo(fmt::format_string<Args...> fmt, Args&&... args) {
    Engine::EngineLogInfo(fmt::format(fmt, std::forward<Args>(args)...).c_str());
}

void LogWarn(const char* msg);
void LogError(const char* msg);
void LogCritical(const char* msg);
} // HotReloading::Engine

using HotReloading::Game::LogInfo;

#endif //WILLENGINETESTBED_ENGINE_LOGGING_H