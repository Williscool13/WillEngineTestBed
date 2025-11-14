//
// Created by William on 2025-11-11.
//

#include "game.h"

#include "game_logging.h"
#include "game_state.h"
#include "input/input.h"

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
    LOG_INFO("Frame {}", -state->frame);
    if (EngineIsKeyPressed(Key::O)) {
        LOG_INFO("Frame {}", state->frame);
    }
}

void GameShutdown(HotReloading::Game::GameState* state)
{
}
