//
// Created by William on 2025-11-11.
//

#ifndef WILLENGINETESTBED_GAME_STATE_H
#define WILLENGINETESTBED_GAME_STATE_H
#include <cstdint>
#include <memory>

#include "spdlog/logger.h"

namespace HotReloading::Game
{
struct GameState
{
    uint64_t frame{0};
    std::shared_ptr<spdlog::logger> logger;
};
} // Hotreloading

#endif //WILLENGINETESTBED_GAME_STATE_H