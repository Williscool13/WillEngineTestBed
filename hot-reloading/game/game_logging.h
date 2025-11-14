//
// Created by William on 2025-11-14.
//

#ifndef WILLENGINETESTBED_ENGINE_LOGGING_H
#define WILLENGINETESTBED_ENGINE_LOGGING_H

#include <fmt/format.h>
#include "hot-reloading/engine/engine_api.h"

namespace HotReloading::Game
{
template<typename... Args>
void LOG_TRACE(fmt::format_string<Args...> fmt, Args&&... args) {
    EngineLogTrace(fmt::format(fmt, std::forward<Args>(args)...).c_str());
}
template<typename... Args>
void LOG_DEBUG(fmt::format_string<Args...> fmt, Args&&... args) {
    EngineLogDebug(fmt::format(fmt, std::forward<Args>(args)...).c_str());
}
template<typename... Args>
void LOG_INFO(fmt::format_string<Args...> fmt, Args&&... args) {
    EngineLogInfo(fmt::format(fmt, std::forward<Args>(args)...).c_str());
}
template<typename... Args>
void LOG_WARN(fmt::format_string<Args...> fmt, Args&&... args) {
    EngineLogWarn(fmt::format(fmt, std::forward<Args>(args)...).c_str());
}
template<typename... Args>
void LOG_ERROR(fmt::format_string<Args...> fmt, Args&&... args) {
    EngineLogError(fmt::format(fmt, std::forward<Args>(args)...).c_str());
}
template<typename... Args>
void LOG_CRITICAL(fmt::format_string<Args...> fmt, Args&&... args) {
    EngineLogCritical(fmt::format(fmt, std::forward<Args>(args)...).c_str());
}
} // HotReloading::Engine


using HotReloading::Game::LOG_TRACE;
using HotReloading::Game::LOG_DEBUG;
using HotReloading::Game::LOG_INFO;
using HotReloading::Game::LOG_WARN;
using HotReloading::Game::LOG_ERROR;
using HotReloading::Game::LOG_CRITICAL;
#endif //WILLENGINETESTBED_ENGINE_LOGGING_H