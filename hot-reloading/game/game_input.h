//
// Created by William on 2025-11-14.
//

#ifndef WILLENGINETESTBED_GAME_INPUT_H
#define WILLENGINETESTBED_GAME_INPUT_H

#include <glm/glm.hpp>

#include "hot-reloading/engine/engine_api.h"
#include "input/input.h"

namespace HotReloading::Input
{
inline bool IsKeyPressed(Key key) { return EngineIsKeyPressed(key); }
inline bool IsKeyReleased(Key key) { return EngineIsKeyReleased(key); }
inline bool IsKeyDown(Key key) { return EngineIsKeyDown(key); }
inline bool IsMousePressed(MouseButton mouseButton) { return EngineIsMousePressed(mouseButton); }
inline bool IsMouseReleased(MouseButton mouseButton) { return EngineIsMouseReleased(mouseButton); }
inline bool IsMouseDown(MouseButton mouseButton) { return EngineIsMouseDown(mouseButton); }

inline glm::vec2 GetMousePosition()
{
    auto pos = EngineGetMousePosition();
    return {pos.x, pos.y};
}

inline glm::vec2 GetMousePositionAbsolute()
{
    auto pos = EngineGetMousePositionAbsolute();
    return {pos.x, pos.y};
}

inline float GetMouseXDelta() { return EngineGetMouseXDelta(); }
inline float GetMouseYDelta() { return EngineGetMouseYDelta(); }
inline float GetMouseWheelDelta() { return EngineGetMouseWheelDelta(); }
inline bool IsCursorActive() { return EngineIsCursorActive(); }
inline bool IsWindowInputFocus() { return EngineIsWindowInputFocus(); }
} // HotReloading::Input

#endif //WILLENGINETESTBED_GAME_INPUT_H
