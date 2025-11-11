//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_MULTIBUFFERING_H
#define WILLENGINETESTBED_MULTIBUFFERING_H

#include <memory>
#include <SDL3/SDL.h>

#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "utils/utils.h"


namespace Renderer
{
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

namespace Template
{
class ClassName
{
public:
    ClassName();

    ~ClassName();

    void Initialize();

    void Run();

    void Render(Renderer::FrameSynchronization& frameSync);

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> vulkanContext{};
    std::unique_ptr<Renderer::Swapchain> swapchain{};
    std::vector<Renderer::FrameSynchronization> frameSynchronization;
    uint64_t frameNumber{0};
    uint32_t renderFramesInFlight{0};

    bool bShouldExit{false};

    // Render Information
    bool bSwapchainOutdated{false};

};
}


#endif //WILLENGINETESTBED_MULTIBUFFERING_H
