//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_RENDERER_H
#define WILLENGINETESTBED_RENDERER_H

#include <memory>
#include <SDL3/SDL.h>

#include "imgui_wrapper.h"
#include "vulkan_context.h"

namespace Renderer
{
class Renderer
{
public:
    Renderer() = default;

    void Initialize();

    void Run();

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<VulkanContext> vulkanContext{nullptr};
    std::unique_ptr<ImguiWrapper> imgui{nullptr};

    bool bShouldExit;
};
}


#endif //WILLENGINETESTBED_RENDERER_H
