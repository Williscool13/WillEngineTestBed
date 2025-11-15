//
// Created by William on 2025-11-13.
//

#ifndef WILLENGINETESTBED_ENGINE_API_H
#define WILLENGINETESTBED_ENGINE_API_H
#include "hot-reloading/asset-load/asset_load_types.h"
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

    Vec2(const glm::vec2& v) : x(v.x), y(v.y) {}
    operator glm::vec2() const { return {x, y,}; }
};

struct Vec3
{
    float x;
    float y;
    float z;

    Vec3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}
    operator glm::vec3() const { return {x, y, z}; }
};

struct Vec4
{
    float x;
    float y;
    float z;
    float w;

    Vec4(const glm::vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
    Vec4(const glm::quat& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
    operator glm::vec4() const { return {x, y, z, w}; }
    operator glm::quat() const { return {w, x, y, z}; }
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

ENGINE_API HotReloading::AssetLoad::RequestLoad EngineLoadModel(const char* path);

ENGINE_API bool EngineUnloadModel(HotReloading::AssetLoad::ModelEntryHandle modelEntryHandle);

ENGINE_API HotReloading::AssetLoad::RuntimeMeshHandle EngineGenerateModel(HotReloading::AssetLoad::ModelEntryHandle model, Vec3 translation, Vec4 quatRotation, Vec3 scale);

ENGINE_API bool EngineUpdateModelTransform(HotReloading::AssetLoad::RuntimeMeshHandle runtimeMeshHandle, Vec3 translation, Vec4 quatRotation, Vec3 scale);

ENGINE_API void EngineDeleteModel(HotReloading::AssetLoad::RuntimeMeshHandle runtimeMeshHandle);

ENGINE_API void EngineUpdateCamera(Vec3 cameraPos, Vec3 cameraLook, Vec3 cameraUp, float fovDegrees, float nearPlane, float farPlane);
#endif //WILLENGINETESTBED_ENGINE_API_H
