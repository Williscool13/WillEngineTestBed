//
// Created by William on 2025-11-13.
//

#include "engine_api.h"

#include "crash-handling/logger.h"

namespace HotReloading::Engine
{
extern "C" ENGINE_API void EngineLogInfo(const char* msg) { LOG_INFO(msg); }
extern "C" ENGINE_API void EngineLogWarn(const char* msg) { LOG_WARN(msg); }
}