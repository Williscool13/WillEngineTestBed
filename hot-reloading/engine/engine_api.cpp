//
// Created by William on 2025-11-13.
//

#include "engine_api.h"

#include "crash-handling/logger.h"
#include "input/input.h"

void EngineLogTrace(const char* msg)
{
    Logger::Get()->trace(msg);
}

void EngineLogDebug(const char* msg)
{
    Logger::Get()->debug(msg);
}

void EngineLogInfo(const char* msg)
{
    Logger::Get()->info(msg);
}

void EngineLogWarn(const char* msg)
{
    Logger::Get()->warn(msg);
}

void EngineLogError(const char* msg)
{
    Logger::Get()->error(msg);
}

void EngineLogCritical(const char* msg)
{
    Logger::Get()->critical(msg);
}

bool EngineIsKeyPressed(Key key)
{
    return ::Input::Get().IsKeyPressed(key);
}

bool EngineIsKeyReleased(Key key)
{
    return ::Input::Get().IsKeyReleased(key);
}

bool EngineIsKeyDown(Key key)
{
    return ::Input::Get().IsKeyDown(key);
}

bool EngineIsMousePressed(MouseButton mouseButton)
{
    return ::Input::Get().IsMousePressed(mouseButton);
}

bool EngineIsMouseReleased(MouseButton mouseButton)
{
    return ::Input::Get().IsMouseReleased(mouseButton);
}

bool EngineIsMouseDown(MouseButton mouseButton)
{
    return ::Input::Get().IsMouseDown(mouseButton);
}

Vec2 EngineGetMousePosition()
{
    glm::vec2 v = ::Input::Get().GetMousePosition();
    return {v.x, v.y};
}

Vec2 EngineGetMousePositionAbsolute()
{
    glm::vec2 v = ::Input::Get().GetMousePositionAbsolute();
    return {v.x, v.y};
}

float EngineGetMouseXDelta()
{
    return ::Input::Get().GetMouseXDelta();
}

float EngineGetMouseYDelta()
{
    return ::Input::Get().GetMouseYDelta();
}

float EngineGetMouseWheelDelta()
{
    return ::Input::Get().GetMouseWheelDelta();
}

bool EngineIsCursorActive()
{
    return ::Input::Get().IsCursorActive();
}

bool EngineIsWindowInputFocus()
{
    return ::Input::Get().IsWindowInputFocus();
}
