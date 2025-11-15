//
// Created by William on 2025-11-13.
//

#include "engine_api.h"

#include "engine.h"
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
    return Input::Get().IsKeyPressed(key);
}

bool EngineIsKeyReleased(Key key)
{
    return Input::Get().IsKeyReleased(key);
}

bool EngineIsKeyDown(Key key)
{
    return Input::Get().IsKeyDown(key);
}

bool EngineIsMousePressed(MouseButton mouseButton)
{
    return Input::Get().IsMousePressed(mouseButton);
}

bool EngineIsMouseReleased(MouseButton mouseButton)
{
    return Input::Get().IsMouseReleased(mouseButton);
}

bool EngineIsMouseDown(MouseButton mouseButton)
{
    return Input::Get().IsMouseDown(mouseButton);
}

Vec2 EngineGetMousePosition()
{
    return Input::Get().GetMousePosition();
}

Vec2 EngineGetMousePositionAbsolute()
{
    return Input::Get().GetMousePositionAbsolute();
}

float EngineGetMouseXDelta()
{
    return Input::Get().GetMouseXDelta();
}

float EngineGetMouseYDelta()
{
    return Input::Get().GetMouseYDelta();
}

float EngineGetMouseWheelDelta()
{
    return Input::Get().GetMouseWheelDelta();
}

bool EngineIsCursorActive()
{
    return Input::Get().IsCursorActive();
}

bool EngineIsWindowInputFocus()
{
    return Input::Get().IsWindowInputFocus();
}

HotReloading::AssetLoad::RequestLoad EngineLoadModel(const char* path)
{
    return HotReloading::Engine::Engine::Get().RequestModelLoad(path);
}

bool EngineUnloadModel(HotReloading::AssetLoad::ModelEntryHandle modelEntryHandle)
{
    return true;
}

HotReloading::AssetLoad::RuntimeMeshHandle EngineGenerateModel(const HotReloading::AssetLoad::ModelEntryHandle model, const Vec3 translation, const Vec4 quatRotation, const Vec3 scale)
{
    return HotReloading::Engine::Engine::Get().GenerateModel(model, {translation, quatRotation, scale});
}

bool EngineUpdateModelTransform(const HotReloading::AssetLoad::RuntimeMeshHandle runtimeMeshHandle, const Vec3 translation, const Vec4 quatRotation, const Vec3 scale)
{
    return HotReloading::Engine::Engine::Get().UpdateRuntimeMesh(runtimeMeshHandle, {translation, quatRotation, scale});
}

void EngineDeleteModel(HotReloading::AssetLoad::RuntimeMeshHandle runtimeMeshHandle)
{}

void EngineUpdateCamera(Vec3 cameraPos, Vec3 cameraLook, Vec3 cameraUp, float fovDegrees, float nearPlane, float farPlane)
{
    return HotReloading::Engine::Engine::Get().UpdateCamera(cameraPos, cameraLook, cameraUp, fovDegrees, nearPlane, farPlane);
}
