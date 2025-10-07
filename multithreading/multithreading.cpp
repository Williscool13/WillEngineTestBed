//
// Created by William on 2025-10-07.
//

#include "multithreading.h"

#include "SDL3/SDL.h"

#include "src/crash-handling/logger.h"

void Multithreading::Initialize()
{
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Multithreading Tests",
        800,
        600,
        window_flags);
}

void Multithreading::Run()
{
    constexpr auto ms = std::chrono::milliseconds(16);


    uint32_t frame = 0;
    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        fmt::println("Frame {}", frame++);
        std::this_thread::sleep_for(ms);

        if (exit) {
            break;
        }
    }
}

void Multithreading::Cleanup()
{
    SDL_DestroyWindow(window);
}
