//
// Created by William on 2025-11-06.
//

#include "free_camera.h"

#include "input/input.h"
#include "utils/world_constants.h"

namespace Game
{
FreeCamera::FreeCamera() : Camera() {}

FreeCamera::FreeCamera(glm::vec3 startingPosition, glm::vec3 startingLookPoint)
{
    transform.translation = startingPosition;
    glm::vec3 forward = glm::normalize(startingLookPoint - startingPosition);
    glm::vec3 right = glm::normalize(glm::cross(forward, WORLD_UP));
    glm::vec3 up = glm::cross(right, forward);
    glm::mat3 rotMatrix(right, up, -forward);
    transform.rotation = glm::quat_cast(rotMatrix);
}

void FreeCamera::Update(float deltaTime)
{
    const Input& input = Input::Get();

    if (!input.IsCursorActive()) {
        return;
    }

    glm::vec3 velocity{0.f};
    float verticalVelocity{0.f};

    if (input.IsKeyDown(Key::D)) {
        velocity.x += 1.0f;
    }
    if (input.IsKeyDown(Key::A)) {
        velocity.x -= 1.0f;
    }
    if (input.IsKeyDown(Key::LCTRL)) {
        verticalVelocity -= 1.0f;
    }
    if (input.IsKeyDown(Key::SPACE)) {
        verticalVelocity += 1.0f;
    }
    if (input.IsKeyDown(Key::W)) {
        velocity.z += 1.0f;
    }
    if (input.IsKeyDown(Key::S)) {
        velocity.z -= 1.0f;
    }

    if (input.IsKeyPressed(Key::RIGHTBRACKET)) {
        speed += 1;
    }
    if (input.IsKeyPressed(Key::LEFTBRACKET)) {
        speed -= 1;
    }
    speed = glm::clamp(speed, -2.0f, 3.0f);

    float scale = speed;
    if (scale <= 0) {
        scale -= 1;
    }
    const float currentSpeed = static_cast<float>(glm::pow(10, scale));

    velocity *= deltaTime * currentSpeed;
    verticalVelocity *= deltaTime * currentSpeed;

    const float yaw = glm::radians(-input.GetMouseXDelta() / 10.0f);
    const float pitch = glm::radians(-input.GetMouseYDelta() / 10.0f);

    const glm::quat currentRotation = transform.rotation;
    const glm::vec3 forward = currentRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const float currentPitch = std::asin(forward.y);

    const float newPitch = glm::clamp(currentPitch + pitch, glm::radians(-89.9f), glm::radians(89.9f));
    const float pitchDelta = newPitch - currentPitch;

    const glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat pitchQuat = glm::angleAxis(pitchDelta, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::quat newRotation = yawQuat * currentRotation * pitchQuat;
    transform.rotation = glm::normalize(newRotation);

    const glm::vec3 right = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 forwardDir = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 finalVelocity = right * velocity.x + forwardDir * velocity.z;
    finalVelocity += glm::vec3(0.0f, verticalVelocity, 0.0f);

    transform.translation += finalVelocity;
}
} // Game
