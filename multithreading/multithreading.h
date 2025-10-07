//
// Created by William on 2025-10-07.
//

#ifndef WILLENGINETESTBED_MULTITHREADING_H
#define WILLENGINETESTBED_MULTITHREADING_H
#include <atomic>
#include <semaphore>
#include <vector>

#include "types.h"


class Multithreading
{
public:
    Multithreading() = default;

    void RenderThread();

    void WatchdogThread();

    void Initialize();

    void Run();

    void Cleanup();

private:
    struct SDL_Window* window{nullptr};

    std::atomic<bool> bShouldExit{false};

    static constexpr int32_t bufferCount = 2;
    std::vector<FrameData> frameBuffers{bufferCount};
    std::counting_semaphore<bufferCount> availableBuffers{bufferCount};
    std::binary_semaphore frameReady{0};


    std::jthread renderThread;

    std::atomic<uint32_t> lastGameFrame{0};
    std::atomic<uint32_t> lastRenderFrame{0};
    // Ridiculous wait_for use with a mutex
    std::condition_variable watchdogCv;
    std::mutex watchdogMutex;
    std::jthread watchdogThread;
    static constexpr int32_t watchdogThreshold{8}; // seconds
};


#endif //WILLENGINETESTBED_MULTITHREADING_H
