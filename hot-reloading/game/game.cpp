//
// Created by William on 2025-11-11.
//

#include "game.h"

#include "game_camera.h"
#include "game_input.h"
#include "game_logging.h"
#include "game_state.h"

void GameInit(HotReloading::Game::GameState* state)
{
    LOG_TRACE("Game Init");
    LOG_DEBUG("Game Init");
    LOG_INFO("Game Init");
    LOG_WARN("Game Init");
    LOG_ERROR("Game Init");
    LOG_CRITICAL("Game Init");
}

void GameUpdate(HotReloading::Game::GameState* state, float deltaTime)
{
    if (HotReloading::Input::IsKeyPressed(Key::NUM_1)) {
        //auto suzannePath = std::filesystem::path("../assets/Suzanne/glTF/Suzanne.gltf");
        const char* structurePath = "../assets/structure.glb";
        const HotReloading::AssetLoad::RequestLoad newModel = EngineLoadModel(structurePath);
        if (!newModel.bIsLoaded) {
            LOG_INFO("Not loaded, will need to wait.");
            uint32_t index = newModel.modelEntryHandle.index;
            uint32_t generation = newModel.modelEntryHandle.generation;
            LOG_INFO("New model: {}, {}", index, generation);
        }
        state->modelEntryHandle = newModel.modelEntryHandle;
    }

    if (HotReloading::Input::IsKeyPressed(Key::Q)) {
        if (state->modelEntryHandle != HotReloading::AssetLoad::ModelEntryHandle::Invalid
            && state->runtimeMeshHandle == HotReloading::AssetLoad::RuntimeMeshHandle::Invalid) {

            state->runtimeMeshHandle = EngineGenerateModel(state->modelEntryHandle, Transform::Identity.translation, Transform::Identity.rotation, Transform::Identity.scale);
            LOG_INFO("Sent Suzanne to be drawn by GPU");
        }
    }

    if (HotReloading::Input::IsKeyPressed(Key::T)) {
        if (state->modelEntryHandle != HotReloading::AssetLoad::ModelEntryHandle::Invalid
            && state->runtimeMeshHandle != HotReloading::AssetLoad::RuntimeMeshHandle::Invalid) {

            Transform t{Transform::Identity};
            t.translation = {0.0f, 10.0f, 0.0f};
            EngineUpdateModelTransform(state->runtimeMeshHandle, t.translation, t.rotation, t.scale);
            LOG_INFO("Sent Suzanne moved 1 unit up");
        }
    }

    UpdateFreeCamera(state->freeCamera, deltaTime);
    HotReloading::Api::Camera::UpdateCamera(state->freeCamera);
}

void GameShutdown(HotReloading::Game::GameState* state)
{}


void UpdateFreeCamera(Game::FreeCamera& camera, float deltaTime)
{
    if (!HotReloading::Input::IsCursorActive()) {
        return;
    }

    glm::vec3 velocity{0.f};
    float verticalVelocity{0.f};

    if (HotReloading::Input::IsKeyDown(Key::D)) {
        velocity.x += 1.0f;
    }
    if (HotReloading::Input::IsKeyDown(Key::A)) {
        velocity.x -= 1.0f;
    }
    if (HotReloading::Input::IsKeyDown(Key::LCTRL)) {
        verticalVelocity -= 1.0f;
    }
    if (HotReloading::Input::IsKeyDown(Key::SPACE)) {
        verticalVelocity += 1.0f;
    }
    if (HotReloading::Input::IsKeyDown(Key::W)) {
        velocity.z += 1.0f;
    }
    if (HotReloading::Input::IsKeyDown(Key::S)) {
        velocity.z -= 1.0f;
    }

    if (HotReloading::Input::IsKeyPressed(Key::RIGHTBRACKET)) {
        camera.speed += 1;
    }
    if (HotReloading::Input::IsKeyPressed(Key::LEFTBRACKET)) {
        camera.speed -= 1;
    }
    camera.speed = glm::clamp(camera.speed, -2.0f, 3.0f);

    float scale = camera.speed;
    if (scale <= 0) {
        scale -= 1;
    }
    const float currentSpeed = static_cast<float>(glm::pow(10, scale));

    velocity *= deltaTime * currentSpeed;
    verticalVelocity *= deltaTime * currentSpeed;

    const float yaw = glm::radians(-HotReloading::Input::GetMouseXDelta() / 10.0f);
    const float pitch = glm::radians(-HotReloading::Input::GetMouseYDelta() / 10.0f);

    const glm::quat currentRotation = camera.transform.rotation;
    const glm::vec3 forward = currentRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const float currentPitch = std::asin(forward.y);

    const float newPitch = glm::clamp(currentPitch + pitch, glm::radians(-89.9f), glm::radians(89.9f));
    const float pitchDelta = newPitch - currentPitch;

    const glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat pitchQuat = glm::angleAxis(pitchDelta, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::quat newRotation = yawQuat * currentRotation * pitchQuat;
    camera.transform.rotation = glm::normalize(newRotation);

    const glm::vec3 right = camera.transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 forwardDir = camera.transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 finalVelocity = right * velocity.x + forwardDir * velocity.z;
    finalVelocity += glm::vec3(0.0f, verticalVelocity, 0.0f);

    camera.transform.translation += finalVelocity;
}