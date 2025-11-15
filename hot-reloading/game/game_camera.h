//
// Created by William on 2025-11-15.
//

#ifndef WILLENGINETESTBED_GAME_CAMERA_H
#define WILLENGINETESTBED_GAME_CAMERA_H

#include <glm/glm.hpp>

#include "game/camera/camera.h"

namespace HotReloading::Api::Camera
{
    void UpdateCamera(glm::vec3 cameraPos, glm::vec3 cameraLook, glm::vec3 cameraUp, float fov, float nearPlane, float farPlane);
    void UpdateCamera(const ::Game::Camera& camera);
} // HotReloading::Camera

#endif //WILLENGINETESTBED_GAME_CAMERA_H