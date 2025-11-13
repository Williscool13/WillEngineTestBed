//
// Created by William on 2025-11-12.
//

#include "engine.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "crash-handling/crash_context.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "core/constants.h"
#include "core/time.h"
#include "input/input.h"
#include "utils/utils.h"

#include "hot-reloading/game/game_state.h"

namespace HotReloading::Engine
{
Engine::Engine() = default;

Engine::~Engine() = default;

void Engine::Initialize()
{
    fmt::println("=== Hot Reloading ===");
    start = std::chrono::high_resolution_clock::now();

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/hot-reloading.log");

    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow(
        "Template",
        ::Core::DEFAULT_WINDOW_WIDTH,
        ::Core::DEFAULT_WINDOW_HEIGHT,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    Input::Get().Init(window, w, h);


    gameState.logger = Logger::Get();

    gameDll.Load("game/hot-reload-game.dll", "hot-reload-game_temp.dll");
    auto gameInit = gameDll.GetFunction<void(*)(Game::GameState* state)>("GameInit");
    if (gameInit) { gameInit(&gameState); }

    renderThread.Initialize(&engineSynchronization, window, w, h);
}

void Engine::Run()
{
    Utils::SetThreadName("GameThread");

    renderThread.Start();
    // assetLoadingThread.Start();

    Input& input = Input::Input::Get();
    Time& time = Time::Get();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    LOG_INFO("Engine Multithreading initialized in {:.3f}s", duration.count() / 1000000.0);

    SDL_Event e;
    bool exit = false;
    while (true) {
        input.FrameReset();
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
            if (e.type == SDL_EVENT_WINDOW_MINIMIZED
                || e.type == SDL_EVENT_WINDOW_RESTORED
                || e.type == SDL_EVENT_WINDOW_MAXIMIZED
                || e.type == SDL_EVENT_WINDOW_RESIZED
                || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bSwapchainOutdated = true;
            }
        }

        SDL_WindowFlags windowFlags = SDL_GetWindowFlags(window);
        input.UpdateFocus(windowFlags);
        time.Update();

        if (exit) {
            renderThread.RequestShutdown();
            engineSynchronization.renderFrames.release();
            //assetLoadingThread.RequestShutdown();
            break;
        }


        // assetLoadingThread.ResolveLoads(loadedModelsToAcquire);
        auto gameUpdate = gameDll.GetFunction<void(*)(Game::GameState* state, float)>("GameUpdate");
        if (gameUpdate) { gameUpdate(&gameState, 0.0f); }

        bool canTransmit = engineSynchronization.gameFrames.try_acquire();
        if (canTransmit) {
            uint64_t currentRenderFrame = renderFrame % ::Core::FRAMES_IN_FLIGHT;
            //PrepareFrameDataForRender(currentRenderFrame);
            renderFrame++;
            engineSynchronization.renderFrames.release();
        }

        gameFrame++;
    }
}

void Engine::Cleanup()
{
    renderThread.Join();
    // assetLoadingThread.Join();

    SDL_DestroyWindow(window);

    auto gameShutdown = gameDll.GetFunction<void(*)(Game::GameState* state)>("GameShutdown");
    if (gameShutdown) { gameShutdown(&gameState); }

    gameDll.Unload();

    SDL_DestroyWindow(window);
}
} // HotReloading::Engine
