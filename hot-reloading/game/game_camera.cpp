//
// Created by William on 2025-11-15.
//

#include "game_camera.h"

#include "game/camera/camera.h"
#include "hot-reloading/engine/engine_api.h"
#include "utils/world_constants.h"

namespace HotReloading::Api::Camera
{
void UpdateCamera(glm::vec3 cameraPos, glm::vec3 cameraLook, glm::vec3 cameraUp, float fov, float nearPlane, float farPlane)
{
    EngineUpdateCamera(cameraPos, cameraLook, cameraUp, fov, nearPlane, farPlane);
}

void UpdateCamera(const Game::Camera& camera)
{
    const glm::vec3 cameraPos = camera.transform.translation;
    const glm::vec3 forward = camera.GetForward();
    const glm::vec3 up = camera.GetUp();
    EngineUpdateCamera(cameraPos, cameraPos + forward, up, glm::degrees(camera.fov), camera.nearPlane, camera.farPlane);
}
} // HotReloading::Camera
