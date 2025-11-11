//
// Created by William on 2025-11-11.
//

#include "hot-reloading.h"

#include <Windows.h>

#include "game/game_state.h"

#include "core/constants.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "game/game.h"
#include "input/input.h"
#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "utils/utils.h"

namespace HotReloading
{
HotReloading::HotReloading() = default;

HotReloading::~HotReloading() = default;

void HotReloading::Initialize()
{
    Utils::ScopedTimer timer{"Hot-Reloading Initialization"};
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow(
        "Template",
        Core::DEFAULT_WINDOW_WIDTH,
        Core::DEFAULT_WINDOW_HEIGHT,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    Input::Get().Init(window, w, h);

    vulkanContext = std::make_unique<Renderer::VulkanContext>(window);
    swapchain = std::make_unique<Renderer::Swapchain>(vulkanContext.get(), w, h);
    renderFramesInFlight = swapchain->imageCount;
    frameSynchronization.reserve(renderFramesInFlight);
    for (int32_t i = 0; i < renderFramesInFlight; ++i) {
        frameSynchronization.emplace_back(vulkanContext.get());
        frameSynchronization[i].Initialize();
    }

    gameState = std::make_unique<Game::GameState>();
    gameState->logger = Logger::Get();

    gameDll.Load("hot-reload-game.dll", "hot-reload-game_temp.dll");
    auto gameInit = gameDll.GetFunction<void(*)(Game::GameState* state)>("GameInit");
    if (gameInit) { gameInit(gameState.get()); }
}

void HotReloading::HotReloading::Run()
{
    Input& input = Input::Input::Get();
    SDL_Event e;
    bool exit = false;
    while (true) {
        auto wait = std::chrono::milliseconds(500);
        std::this_thread::sleep_for(wait);

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
        input.UpdateFocus(SDL_GetWindowFlags(window));

        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            for (Renderer::FrameSynchronization& frameSync : frameSynchronization) {
                frameSync.RecreateSynchronization();
            }

            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            bSwapchainOutdated = false;
        }

        if (exit) {
            bShouldExit = true;
            break;
        }

        gameState->frame = frameNumber;

        auto gameUpdate = gameDll.GetFunction<void(*)(Game::GameState* state, float)>("GameUpdate");
        if (gameUpdate) { gameUpdate(gameState.get(), 0.0f); }
        // auto& currentFrameSync = frameSynchronization[frameNumber % renderFramesInFlight];
        // Render(currentFrameSync);

        LOG_INFO("Frame {}", frameNumber);
        frameNumber++;
    }
}

void HotReloading::Render(Renderer::FrameSynchronization& frameSync)
{
    // stub
}

void HotReloading::HotReloading::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);
 
    auto gameShutdown = gameDll.GetFunction<void(*)(Game::GameState* state)>("GameShutdown");
    if (gameShutdown) { gameShutdown(gameState.get()); }

    SDL_DestroyWindow(window);
}
}
