//
// Created by William on 2025-11-13.
//

#ifndef WILLENGINETESTBED_ENGINE_SYNCHRONIZATION_H
#define WILLENGINETESTBED_ENGINE_SYNCHRONIZATION_H
#include <semaphore>

#include "core/constants.h"
#include "render/render-operations/render_operations.h"

namespace HotReloading::Engine
{
struct EngineSynchronization
{
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> gameFrames{Core::FRAMES_IN_FLIGHT};
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> renderFrames{0};

    std::array<Renderer::FrameBuffer, Core::FRAMES_IN_FLIGHT> frameBuffers{};
};
} // HotReloading::Engine

#endif //WILLENGINETESTBED_ENGINE_SYNCHRONIZATION_H
