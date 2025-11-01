//
// Created by William on 2025-10-31.
//

#include "render_thread.h"

#include <VkBootstrap.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>

#include "engine_multithreading.h"
#include "offsetAllocator.hpp"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_imgui_wrapper.h"
#include "render/vk_descriptors.h"
#include "render/vk_pipelines.h"
#include "render/vk_helpers.h"
#include "render/render_context.h"
#include "render/render_utils.h"
#include "render/render_constants.h"

#include "render/descriptor_buffer/descriptor_buffer_combined_image_sampler.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/descriptor_buffer/descriptor_buffer_uniform.h"

#include "input/input.h"
#include "../src/render/model/model_loader.h"
#include "core/time.h"
#include "render/render_targets.h"
#include "utils/utils.h"
#include "utils/world_constants.h"

namespace Renderer
{
RenderThread::RenderThread() = default;

RenderThread::~RenderThread()
{
    if (thread.joinable()) {
        RequestShutdown();
        Join();
    }
}

void RenderThread::Initialize(EngineMultithreading* engineMultithreading_, SDL_Window* window_, uint32_t w, uint32_t h)
{
    Utils::ScopedTimer timer{"[Render Thread] Render thread initialization"};

    engineMultithreading = engineMultithreading_;
    window = window_;

    renderContext = std::make_unique<RenderContext>(w, h, DEFAULT_RENDER_SCALE);
    std::array<uint32_t, 2> renderExtent = renderContext->GetRenderExtent();

    vulkanContext = std::make_unique<VulkanContext>(window);
    swapchain = std::make_unique<Swapchain>(vulkanContext.get(), renderExtent[0], renderExtent[1]);
    // Imgui multithreaded needs a lot more attention, see
    // https://github.com/ocornut/imgui/issues/1860#issuecomment-1927630727
    imgui = std::make_unique<ImguiWrapper>(vulkanContext.get(), window, swapchain->imageCount, swapchain->format);
    renderTargets = std::make_unique<RenderTargets>(vulkanContext.get(), w, h);
    Input::Input::Get().Init(window, swapchain->extent.width, swapchain->extent.height);

    for (FrameSynchronization& frameSync : frameSynchronization) {
        frameSync = FrameSynchronization(vulkanContext.get());
        frameSync.Initialize();
    }

    //modelLoader = std::make_unique<ModelLoader>(vulkanContext.get());

    for (AllocatedBuffer& sceneBuffer : sceneDataBuffers) {
        VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.pNext = nullptr;
        VmaAllocationCreateInfo vmaAllocInfo = {};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(SceneData);
        sceneBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    }
}

void RenderThread::Start()
{
    thread = std::jthread(&RenderThread::ThreadMain, this);
}

void RenderThread::RequestShutdown()
{
    bShouldExit.store(true);
}

void RenderThread::Join()
{
    if (thread.joinable()) {
        thread.join();
    }
}

void RenderThread::ThreadMain()
{
    Utils::SetThreadName("Render Thread");

    while (!bShouldExit.load()) {
        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            if (RENDER_TARGET_SIZE_EQUALS_SWAPCHAIN_SIZE) {
                renderContext->RequestRenderExtentResize(w, h);
            }
            bSwapchainOutdated = false;
        }
        if (renderContext->HasPendingRenderExtentChanges()) {
            vkDeviceWaitIdle(vulkanContext->device);
            renderContext->ApplyRenderExtentResize();

            std::array<uint32_t, 2> newExtents = renderContext->GetRenderExtent();
            renderTargets->Recreate(newExtents[0], newExtents[1]);

            // // Upload to descriptor buffer
            // VkDescriptorImageInfo drawDescriptorInfo;
            // drawDescriptorInfo.imageView = renderTargets->drawImageView.handle;
            // drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            // renderTargetDescriptors.UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);
        }

        engineMultithreading->renderFrames.acquire();
        if (bShouldExit.load()) { break; }

        uint32_t currentFrameInFlight = frameNumber % Core::FRAMES_IN_FLIGHT;
        FrameSynchronization& currentFrameData = frameSynchronization[currentFrameInFlight];

        // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
        VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &currentFrameData.renderFence, true, 1000000000));

        auto frameStartTime = std::chrono::high_resolution_clock::now();

        RenderResponse renderResponse = Render(currentFrameInFlight, currentFrameData);
        switch (renderResponse) {
            case RenderResponse::SWAPCHAIN_OUTDATED:
                bSwapchainOutdated = true;
                break;
            default:
                auto frameEndTime = std::chrono::high_resolution_clock::now();
                float frameTimeMs = std::chrono::duration<float, std::milli>(frameEndTime - frameStartTime).count();
                UpdateFrameTimeStats(frameTimeMs);
                break;
        }

        frameNumber++;

        engineMultithreading->gameFrames.release();
    }

    vkDeviceWaitIdle(vulkanContext->device);
}

RenderThread::RenderResponse RenderThread::Render(uint32_t currentFrameInFlight, FrameSynchronization& currentFrameData)
{
    auto timer = Utils::ScopedTimer(fmt::format("[Render Thread] Frame time (Frame {})", frameNumber));

    std::array<uint32_t, 2> scaledRenderExtent = renderContext->GetScaledRenderExtent();
    uint32_t swapchainImageIndex;
    // (Non-Blocking) Acquire swapchain image index. Signal semaphore when the actual image is ready for use.
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, 1000000000, currentFrameData.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return RenderResponse::SWAPCHAIN_OUTDATED;
    }

    // Un-signal fence, essentially saying "I'm using this frame-in-flight's resources, hands off".
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &currentFrameData.renderFence));

    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];
    VkImageView currentSwapchainImageView = swapchain->swapchainImageViews[swapchainImageIndex];


    // Do rendering stuff
    VkCommandBuffer cmd = currentFrameData.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    constexpr float cameraPos[3] = {0, 2, 0};
    constexpr float cameraLook[3] = {0, 0, 0};
    glm::mat4 view = glm::lookAt(
        glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]),
        glm::vec3(cameraLook[0], cameraLook[1], cameraLook[2]),
        WORLD_UP);

    glm::mat4 proj = glm::perspective(
        glm::radians(75.0f),
        static_cast<float>(scaledRenderExtent[0]) / static_cast<float>(scaledRenderExtent[1]),
        1000.0f,
        0.1f);

    sceneData.prevView = sceneData.view;
    sceneData.prevProj = sceneData.proj;
    sceneData.prevViewProj = sceneData.viewProj;
    sceneData.view = view;
    sceneData.proj = proj;
    sceneData.viewProj = proj * view;
    sceneData.renderTargetSize.x = scaledRenderExtent[0];
    sceneData.renderTargetSize.y = scaledRenderExtent[1];

    //sceneData.deltaTime = deltaTime;

    AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
    auto* currentSceneData = static_cast<SceneData*>(currentSceneDataBuffer.allocationInfo.pMappedData);
    *currentSceneData = sceneData;

    // Final transition
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

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
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
        return RenderResponse::SWAPCHAIN_OUTDATED;
    }

    LOG_INFO("[Render Thread] Processed frame {} (should match frame data's frame number - {})", frameNumber, engineMultithreading->frameBuffers[currentFrameInFlight].currentFrame);
    return RenderResponse::OK;
}

void RenderThread::UpdateFrameTimeStats(float frameTimeMs)
{
    frameTimeHistory[frameTimeHistoryIndex] = frameTimeMs;
    frameTimeHistoryIndex = (frameTimeHistoryIndex + 1) % FRAME_TIME_HISTORY_SIZE;

    if (frameTimeHistoryCount < FRAME_TIME_HISTORY_SIZE) {
        frameTimeHistoryCount++;
    }

    float sum = 0.0f;
    for (size_t i = 0; i < frameTimeHistoryCount; i++) {
        sum += frameTimeHistory[i];
    }
    rollingAverageFrameTime = sum / static_cast<float>(frameTimeHistoryCount);


    if (frameTimeHistoryCount >= 10) {
        constexpr float SPIKE_THRESHOLD = 1.5f;

        if (frameTimeMs > rollingAverageFrameTime * SPIKE_THRESHOLD) {
            float percentIncrease = ((frameTimeMs / rollingAverageFrameTime) - 1.0f) * 100.0f;
            LOG_WARN("[Render Thread] Frame {} spike detected: {:.2f}ms (avg: {:.2f}ms, +{:.1f}%)",
                     frameNumber, frameTimeMs, rollingAverageFrameTime, percentIncrease);
        }
    }

    if (frameNumber % 100 == 0 && frameNumber > 0) {
        LOG_INFO("[Render Thread] Rolling average frame time (last {} frames): {:.2f}ms ({:.1f} FPS)",
                 frameTimeHistoryCount, rollingAverageFrameTime, 1000.0f / rollingAverageFrameTime);
    }
}
} // Renderer
