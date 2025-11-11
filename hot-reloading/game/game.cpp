//
// Created by William on 2025-11-11.
//

#include "game.h"

#include "game_state.h"
#include "crash-handling/logger.h"
#include "input/input.h"

namespace HotReloading::Game
{
void GameInit(GameState* state)
{
    state->logger->info("Game init 200");
    //fmt::println("Test");
}

void GameUpdate(GameState* state)
{
    state->logger->info("Game update 10");
    // LOG_INFO("Game is printing whatever");
    // Input i = Input::Get();
    // if (i.IsKeyPressed(Key::O)) {
    //     LOG_INFO("Frame {}", state->frame);
    // }
}

void GameShutdown(GameState* state)
{
    // LOG_INFO("Game shutdown");
}
}
