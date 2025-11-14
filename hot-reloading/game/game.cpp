//
// Created by William on 2025-11-11.
//

#include "game.h"

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
}

void GameShutdown(HotReloading::Game::GameState* state)
{}
