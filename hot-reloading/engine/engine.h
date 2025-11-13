//
// Created by William on 2025-11-12.
//

#ifndef WILLENGINETESTBED_ENGINE_H
#define WILLENGINETESTBED_ENGINE_H
#include <SDL3/SDL.h>

#include "hot-reloading/game/game_state.h"
#include "hot-reloading/render/render.h"
#include "utils/utils.h"

namespace Renderer
{
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

namespace HotReloading::Engine
{
class Engine
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
    EngineSynchronization engineSynchronization{};

private:
    Render::RenderThread renderThread{};

    uint64_t gameFrame{0};
    uint64_t renderFrame{0};

    bool bShouldExit{false};
    bool bSwapchainOutdated{false};

    std::chrono::time_point<std::chrono::steady_clock> start{};


    Utils::DllLoader gameDll;
};
} // HotReloading::Engine

#endif //WILLENGINETESTBED_ENGINE_H
