//
// Created by William on 2025-10-09.
//

#include "renderer.h"

#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"
#include "utils.h"


namespace Renderer
{
void Renderer::Initialize()
{
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Vulkan Test Bed",
        800,
        600,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    vulkanContext = std::make_unique<VulkanContext>(window);
    imgui = std::make_unique<ImguiWrapper>(vulkanContext.get(), window, SWAPCHAIN_IMAGE_FORMAT);
}

void Renderer::Run()
{
    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        if (exit) {
            bShouldExit = true;
            break;
        }

        auto wait = std::chrono::milliseconds(100);
        std::this_thread::sleep_for(wait);
        LOG_INFO("Tick");
    }
}

void Renderer::Cleanup()
{
    SDL_DestroyWindow(window);
}
}
