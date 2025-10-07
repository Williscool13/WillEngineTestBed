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
        return;
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Multithreading Tests",
        800,
        600,
        window_flags);

    frameBuffers.resize(bufferCount);
}

void Multithreading::RenderThread()
{
    int32_t operatingIndex = -1;
    while (!bShouldExit.load()) {
        frameReady.acquire();

        if (bShouldExit.load()) { break; }

        operatingIndex = (operatingIndex + 1) % 2;
        FrameData& buffer = frameBuffers[operatingIndex];

        // Slower than game thread to simulate slower render thread. Game thread should come out ahead after the first few frames.
        constexpr auto renderWait = std::chrono::milliseconds(50);
        std::this_thread::sleep_for(renderWait);
        LOG_INFO("[Render thread] Simulated render logic ({} ms). Frame {}", renderWait.count(), buffer.frameCount);

        availableBuffers.release();
    }
}


void Multithreading::Run()
{
    std::jthread renderThread(&Multithreading::RenderThread, this);

    uint32_t frame = 0;
    SDL_Event e;
    bool exit = false;
    int32_t operatingIndex = -1;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        if (exit) {
            bShouldExit = true;
            frameReady.release();
            break;
        }

        availableBuffers.acquire();
        operatingIndex = (operatingIndex + 1) % 2;
        FrameData& buffer = frameBuffers[operatingIndex];
        buffer.frameCount = frame;


        constexpr auto gameWait = std::chrono::milliseconds(20);
        std::this_thread::sleep_for(gameWait);
        LOG_INFO("[Game Thread] Simulated game logic ({} ms). Frame {}", gameWait.count(), frame++);

        frameReady.release();
    }

    renderThread.join();
}

void Multithreading::Cleanup()
{
    SDL_DestroyWindow(window);
}
