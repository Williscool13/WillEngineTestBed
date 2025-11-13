//
// Created by William on 2025-11-13.
//

#ifndef WILLENGINETESTBED_ENGINE_API_H
#define WILLENGINETESTBED_ENGINE_API_H

#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif

namespace HotReloading::Engine
{
extern "C" {
    ENGINE_API void EngineLogInfo(const char* msg);
    ENGINE_API void EngineLogWarn(const char* msg);
    ENGINE_API void EngineLogError(const char* msg);
}
}

#endif //WILLENGINETESTBED_ENGINE_API_H