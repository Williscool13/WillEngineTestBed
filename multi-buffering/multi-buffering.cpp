//
// Created by William on 2025-10-09.
//

#include "multi-buffering.h"

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
MultiBuffering::MultiBuffering() = default;

MultiBuffering::~MultiBuffering() = default;

void MultiBuffering::Initialize()
{
    Utils::ScopedTimer timer{"MultiBuffering Initialization"};
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
        "Multibuffering",
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

    frameSynchronization.reserve(renderFramesInFlight);
    for (int32_t i = 0; i < renderFramesInFlight; ++i) {
        frameSynchronization.emplace_back(vulkanContext.get());
        frameSynchronization[i].Initialize();
        SetObjectName(vulkanContext->device, (uint64_t) frameSynchronization[i].commandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, fmt::format("Command Buffer {}", i).c_str());
        SetObjectName(vulkanContext->device, (uint64_t) frameSynchronization[i].renderFence, VK_OBJECT_TYPE_FENCE, fmt::format("Render Fence {}", i).c_str());
        SetObjectName(vulkanContext->device, (uint64_t) frameSynchronization[i].renderSemaphore, VK_OBJECT_TYPE_SEMAPHORE, fmt::format("Render Semaphore {}", i).c_str());
        SetObjectName(vulkanContext->device, (uint64_t) frameSynchronization[i].swapchainSemaphore, VK_OBJECT_TYPE_SEMAPHORE, fmt::format("Swapchain Semaphore {}", i).c_str());
    }
}

void MultiBuffering::Run()
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

void MultiBuffering::Render()
{
    const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
    const FrameSynchronization& currentFrameData = frameSynchronization[currentFrameInFlight];

    uint32_t swapchainImageIndex;
    // Acquire swapchain image index. Signal semaphore when the actual image is ready for use.
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, 1000000000, currentFrameData.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return;
    }


    // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &currentFrameData.renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &currentFrameData.renderFence));

    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];

    VkCommandBuffer cmd = currentFrameData.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    VkClearColorValue clear{0.3f, 0.0f, 0.0f, 1.0f};
    VkImageSubresourceRange subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, currentSwapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, &clear, 1, &subresource);

    VK_CHECK(vkEndCommandBuffer(cmd));


    VkCommandBufferSubmitInfo commandBufferSubmitInfo = VkHelpers::CommandBufferSubmitInfo(currentFrameData.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = VkHelpers::SemaphoreSubmitInfo(currentFrameData.swapchainSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
    VkSemaphoreSubmitInfo renderSemaphoreSignalInfo = VkHelpers::SemaphoreSubmitInfo(currentFrameData.renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&commandBufferSubmitInfo, &swapchainSemaphoreWaitInfo, &renderSemaphoreSignalInfo);

    // Wait for swapchain semaphore, then submit command buffer. When finished, signal render semaphore and render fence.
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, currentFrameData.renderFence));

    // Wait for render semaphore, then present frame.
    VkPresentInfoKHR presentInfo = VkHelpers::PresentInfo(&swapchain->handle, nullptr, &swapchainImageIndex);
    presentInfo.pWaitSemaphores = &currentFrameData.renderSemaphore;
    const VkResult presentResult = vkQueuePresentKHR(vulkanContext->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}

void MultiBuffering::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    SDL_DestroyWindow(window);
}

void MultiBuffering::UpdateFrameTimeStats(float frameTimeMs)
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
