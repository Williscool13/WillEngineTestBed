//
// Created by William on 2025-11-13.
//

#ifndef WILLENGINETESTBED_ENGINE_API_H
#define WILLENGINETESTBED_ENGINE_API_H
#include "input/input.h"

#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif

struct Vec2
{
    float x;
    float y;
};

ENGINE_API void EngineLogTrace(const char* msg);

ENGINE_API void EngineLogDebug(const char* msg);

ENGINE_API void EngineLogInfo(const char* msg);

ENGINE_API void EngineLogWarn(const char* msg);

ENGINE_API void EngineLogError(const char* msg);

ENGINE_API void EngineLogCritical(const char* msg);

ENGINE_API bool EngineIsKeyPressed(Key key);

ENGINE_API bool EngineIsKeyReleased(Key key);

ENGINE_API bool EngineIsKeyDown(Key key);

ENGINE_API bool EngineIsMousePressed(MouseButton mouseButton);

ENGINE_API bool EngineIsMouseReleased(MouseButton mouseButton);

ENGINE_API bool EngineIsMouseDown(MouseButton mouseButton);

ENGINE_API Vec2 EngineGetMousePosition();

ENGINE_API Vec2 EngineGetMousePositionAbsolute();

ENGINE_API float EngineGetMouseXDelta();

ENGINE_API float EngineGetMouseYDelta();

ENGINE_API float EngineGetMouseWheelDelta();

ENGINE_API bool EngineIsCursorActive();

ENGINE_API bool EngineIsWindowInputFocus();
#endif //WILLENGINETESTBED_ENGINE_API_H
