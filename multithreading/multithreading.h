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
};


#endif //WILLENGINETESTBED_MULTITHREADING_H