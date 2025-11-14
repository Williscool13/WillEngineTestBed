//
// Created by William on 2025-11-11.
//

#ifndef WILLENGINETESTBED_GAME_H
#define WILLENGINETESTBED_GAME_H

namespace HotReloading::Game
{
struct GameState;
}

#ifdef GAME_EXPORTS
#define GAME_API __declspec(dllexport)
#else
#define GAME_API __declspec(dllimport)
#endif

extern "C" {
    GAME_API void GameInit(HotReloading::Game::GameState* state);
    GAME_API void GameUpdate(HotReloading::Game::GameState* state, float deltaTime);
    GAME_API void GameShutdown(HotReloading::Game::GameState* state);
}




#endif //WILLENGINETESTBED_GAME_H