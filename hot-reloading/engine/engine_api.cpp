//
// Created by William on 2025-11-13.
//

#include "engine_api.h"

#include "crash-handling/logger.h"

namespace HotReloading::Engine
{
extern "C" ENGINE_API void EngineLogTrace(const char* msg)
{
    Logger::Get()->trace(msg);
}
extern "C" ENGINE_API void EngineLogDebug(const char* msg)
{
    Logger::Get()->debug(msg);
}
extern "C" ENGINE_API void EngineLogInfo(const char* msg)
{
    Logger::Get()->info(msg);
}
extern "C" ENGINE_API void EngineLogWarn(const char* msg)
{
    Logger::Get()->warn(msg);
}
extern "C" ENGINE_API void EngineLogError(const char* msg)
{
    Logger::Get()->error(msg);
}
extern "C" ENGINE_API void EngineLogCritical(const char* msg)
{
    Logger::Get()->critical(msg);
}
}