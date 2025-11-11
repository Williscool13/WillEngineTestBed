//
// Created by William on 2025-11-11.
//

#ifndef WILLENGINETESTBED_HOT_RELOADING_H
#define WILLENGINETESTBED_HOT_RELOADING_H
#include <memory>
#include <SDL3/SDL.h>

#include "render/vk_synchronization.h"
#include "utils/utils.h"

namespace Renderer
{
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}


namespace HotReloading
{
namespace Game
{
    struct GameState;
}

class HotReloading
{
public:
    HotReloading();
    ~HotReloading();
    void Initialize();

    void Run();

    void Render(Renderer::FrameSynchronization& frameSync);

    void Cleanup();

private:
    std::unique_ptr<Game::GameState> gameState;

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> vulkanContext{};
    std::unique_ptr<Renderer::Swapchain> swapchain{};
    std::vector<Renderer::FrameSynchronization> frameSynchronization;
    uint64_t frameNumber{0};
    uint32_t renderFramesInFlight{0};


    bool bShouldExit{false};
    bool bSwapchainOutdated{false};

    Utils::DllLoader gameDll;
};
}


#endif //WILLENGINETESTBED_HOT_RELOADING_H
