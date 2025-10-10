//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_RENDERER_H
#define WILLENGINETESTBED_RENDERER_H

#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "synchronization.h"


namespace Renderer
{
class ImguiWrapper;
struct VulkanContext;
struct Swapchain;

class Renderer
{
public:
    Renderer();

    ~Renderer();

    void Initialize();

    void Run();

    void Render();

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<VulkanContext> vulkanContext{};
    std::unique_ptr<Swapchain> swapchain{};
    std::unique_ptr<ImguiWrapper> imgui{};

    uint64_t frameNumber{0};
    std::vector<FrameData> frameSynchronization;

    bool bShouldExit{false};
    bool bWindowChanged{false};
};
}


#endif //WILLENGINETESTBED_RENDERER_H
