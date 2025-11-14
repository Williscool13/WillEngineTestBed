//
// Created by William on 2025-11-12.
//

#ifndef WILLENGINETESTBED_ENGINE_H
#define WILLENGINETESTBED_ENGINE_H
#include <SDL3/SDL.h>

#include <hot-reloading/engine/engine_api.h>
#include "hot-reloading/game/game_state.h"
#include "hot-reloading/render/render.h"

namespace Renderer
{
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

namespace HotReloading::Engine
{
using GameInitFunc = void(*)(Game::GameState*);
using GameUpdateFunc = void(*)(Game::GameState*, float);
using GameShutdownFunc = void(*)(Game::GameState*);

void StubInit(Game::GameState* state);

void StubUpdate(Game::GameState* state, float deltaTime);

void StubShutdown(Game::GameState* state);

struct GameFunctions
{
    GameInitFunc gameInit;
    GameUpdateFunc gameUpdate;
    GameShutdownFunc gameShutdown;

    void Stub()
    {
        gameInit = StubInit;
        gameUpdate = StubUpdate;
        gameShutdown = StubShutdown;
    }
};


class ENGINE_API Engine
{
public:
    Engine();

    ~Engine();

    void Initialize();

    void Run();

    void Cleanup();

private:
    SDL_Window* window{nullptr};

    Game::GameState gameState;
    Core::EngineSynchronization engineSynchronization{};

private:
    Render::RenderThread renderThread{};

    uint64_t gameFrame{0};
    uint64_t renderFrame{0};

    bool bShouldExit{false};
    bool bSwapchainOutdated{false};

    std::chrono::time_point<std::chrono::steady_clock> start{};

private: // Game DLL Loading
    Utils::DllLoader gameDll;
    GameFunctions gameFunctions = {
        StubInit,
        StubUpdate,
        StubShutdown,
    };
};
} // HotReloading::Engine

#endif //WILLENGINETESTBED_ENGINE_H
