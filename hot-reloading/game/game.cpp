//
// Created by William on 2025-11-11.
//

#include "game.h"

#include "game_logging.h"
#include "game_state.h"
#include "crash-handling/logger.h"
#include "hot-reloading/engine/engine_api.h"
#include "input/input.h"

static HotReloading::Engine::EngineApi* g_Engine = nullptr;

namespace HotReloading::Game
{
void GameDllInit(Engine::EngineApi* engineApi)
{
    g_Engine = engineApi;
}
void GameInit(GameState* state)
{
    //state->logger->info("Game init 200");
    //fmt::println("Test");
}

void GameUpdate(GameState* state)
{
    LogInfo();
    LogInfo("Frame {}", state->frame + 100);

    //state->logger->info("Game update");
    // LOG_INFO("Game is printing whatever");
    Input i = Input::Get();
    if (i.IsKeyPressed(Key::O)) {
        LogInfo("Frame {}", state->frame);
    }
}

void GameShutdown(GameState* state)
{
    // LOG_INFO("Game shutdown");
}


}
