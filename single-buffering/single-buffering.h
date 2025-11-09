//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_MULTIBUFFERING_H
#define WILLENGINETESTBED_MULTIBUFFERING_H

#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "render/render_context.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "utils/utils.h"


namespace Renderer
{
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;

class SingleBuffering
{
public:
    SingleBuffering();

    ~SingleBuffering();

    void Initialize();

    void Run();

    void Render();

    void Cleanup();


private:
    void UpdateFrameTimeStats(float frameTimeMs);
    Utils::FrameTimeTracker frameTimeTracker{1000, 1.5f};

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<VulkanContext> vulkanContext{};
    std::unique_ptr<Swapchain> swapchain{};

    uint64_t frameNumber{0};
    FrameSynchronization frameSync;
    int32_t renderFramesInFlight{0};

    bool bShouldExit{false};

    // Render Information
    bool bSwapchainOutdated{false};

};
}


#endif //WILLENGINETESTBED_MULTIBUFFERING_H
