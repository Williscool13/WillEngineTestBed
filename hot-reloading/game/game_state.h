//
// Created by William on 2025-11-11.
//

#ifndef WILLENGINETESTBED_GAME_STATE_H
#define WILLENGINETESTBED_GAME_STATE_H
#include <cstdint>

#include "game/camera/free_camera.h"
#include "hot-reloading/asset-load/asset_load_types.h"

namespace HotReloading::Game
{
struct GameState
{
    uint64_t frame{0};

    AssetLoad::ModelEntryHandle modelEntryHandle{AssetLoad::ModelEntryHandle::Invalid};
    AssetLoad::RuntimeMeshHandle runtimeMeshHandle{AssetLoad::RuntimeMeshHandle::Invalid};
    ::Game::FreeCamera freeCamera{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}};
};
} // Hotreloading

#endif //WILLENGINETESTBED_GAME_STATE_H