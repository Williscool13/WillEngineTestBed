//
// Created by William on 2025-10-09.
//

#include "single-buffering.h"

#include "VkBootstrap.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "core/constants.h"

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_descriptors.h"
#include "render/vk_helpers.h"
#include "render/render_utils.h"
#include "render/render_constants.h"
#include "render/render_targets.h"

#include "input/input.h"
#include "utils/utils.h"

namespace Renderer
{
SingleBuffering::SingleBuffering() = default;

SingleBuffering::~SingleBuffering() = default;

void SingleBuffering::Initialize()
{
    Utils::ScopedTimer timer{"SingleBuffering Initialization"};
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(primaryDisplay);
    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow(
        "SingleBuffering",
        Core::DEFAULT_WINDOW_WIDTH,
        Core::DEFAULT_WINDOW_HEIGHT,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);

    vulkanContext = std::make_unique<VulkanContext>(window);
    swapchain = std::make_unique<Swapchain>(vulkanContext.get(), w, h);
    Input::Input::Get().Init(window, swapchain->extent.width, swapchain->extent.height);

    renderFramesInFlight = swapchain->imageCount;
    frameSync = FrameSynchronization(vulkanContext.get());
    frameSync.Initialize();
}

void SingleBuffering::Run()
{
    Input& input = Input::Input::Get();
    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bSwapchainOutdated = true;
            }
        }
        input.UpdateFocus(SDL_GetWindowFlags(window));

        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            bSwapchainOutdated = false;
        }

        auto frameStartTime = std::chrono::high_resolution_clock::now();

        if (exit) {
            bShouldExit = true;
            break;
        }

        Render();

        auto frameEndTime = std::chrono::high_resolution_clock::now();
        float frameTimeMs = std::chrono::duration<float, std::milli>(frameEndTime - frameStartTime).count();
        UpdateFrameTimeStats(frameTimeMs);
        input.FrameReset();
        frameNumber++;
    }
}

void SingleBuffering::Render()
{
    const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;

    uint32_t swapchainImageIndex;
    // Acquire swapchain image index. Signal semaphore when the actual image is ready for use.
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, 1000000000, frameSync.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return;
    }


    // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &frameSync.renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &frameSync.renderFence));

    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];

    VkCommandBuffer cmd = frameSync.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    VkClearColorValue clear{0.3f, 0.0f, 0.0f, 1.0f};
    VkImageSubresourceRange subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, currentSwapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, &clear, 1, &subresource);

    VK_CHECK(vkEndCommandBuffer(cmd));


    VkCommandBufferSubmitInfo commandBufferSubmitInfo = VkHelpers::CommandBufferSubmitInfo(frameSync.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = VkHelpers::SemaphoreSubmitInfo(frameSync.swapchainSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
    VkSemaphoreSubmitInfo renderSemaphoreSignalInfo = VkHelpers::SemaphoreSubmitInfo(frameSync.renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&commandBufferSubmitInfo, &swapchainSemaphoreWaitInfo, &renderSemaphoreSignalInfo);

    // Wait for swapchain semaphore, then submit command buffer. When finished, signal render semaphore and render fence.
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, frameSync.renderFence));

    // Wait for render semaphore, then present frame.
    VkPresentInfoKHR presentInfo = VkHelpers::PresentInfo(&swapchain->handle, nullptr, &swapchainImageIndex);
    presentInfo.pWaitSemaphores = &frameSync.renderSemaphore;
    const VkResult presentResult = vkQueuePresentKHR(vulkanContext->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}

void SingleBuffering::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    SDL_DestroyWindow(window);
}

void SingleBuffering::UpdateFrameTimeStats(float frameTimeMs)
{
    frameTimeTracker.RecordFrameTime(frameTimeMs);

    if (frameTimeTracker.GetSampleCount() >= 10) {
        float avg = frameTimeTracker.GetRollingAverage();
        if (frameTimeMs > avg * frameTimeTracker.GetSpikeThreshold()) {
            float percentIncrease = ((frameTimeMs / avg) - 1.0f) * 100.0f;
            LOG_WARN("[Render Thread] Frame {} spike detected: {:.2f}ms (avg: {:.2f}ms, +{:.1f}%)",
                     frameNumber, frameTimeMs, avg, percentIncrease);
        }
    }

    if (frameNumber % 1000 == 0 && frameNumber > 0) {
        float avg = frameTimeTracker.GetRollingAverage();
        LOG_INFO("[Render Thread] Rolling average frame time (last {} frames): {:.2f}ms ({:.1f} FPS)",
                 frameTimeTracker.GetSampleCount(), avg, 1000.0f / avg);
    }
}
}
