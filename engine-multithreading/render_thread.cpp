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
#include "render/model/model_loader.h"
#include "core/time.h"
#include "render/render_targets.h"
#include "render/resource_manager.h"
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
    engineMultithreading = engineMultithreading_;
    window = window_; {
        Utils::ScopedTimer timer{"[Render Thread] Render Context"};
        renderContext = std::make_unique<RenderContext>(w, h, DEFAULT_RENDER_SCALE);
        std::array<uint32_t, 2> renderExtent = renderContext->GetRenderExtent();

        vulkanContext = std::make_unique<VulkanContext>(window);
        swapchain = std::make_unique<Swapchain>(vulkanContext.get(), renderExtent[0], renderExtent[1]);
        // Imgui multithreaded needs a lot more attention, see
        // https://github.com/ocornut/imgui/issues/1860#issuecomment-1927630727
        imgui = std::make_unique<ImguiWrapper>(vulkanContext.get(), window, swapchain->imageCount, swapchain->format);
        renderTargets = std::make_unique<RenderTargets>(vulkanContext.get(), w, h);
        resourceManager = std::make_unique<ResourceManager>(vulkanContext.get());
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

    InitializeBuffers();
    InitializeResources();
}

void RenderThread::InitializeBuffers()
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;
    bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * BINDLESS_INSTANCE_COUNT;
    opaqueIndexedIndirectBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;
    bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * BINDLESS_INSTANCE_COUNT;
    opaqueSkeletalIndexedIndirectBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    indirectCountBuffers.reserve(swapchain->imageCount);
    skeletalIndirectCountBuffers.reserve(swapchain->imageCount);
    modelBuffers.reserve(swapchain->imageCount);
    instanceBuffers.reserve(swapchain->imageCount);
    jointMatrixBuffers.reserve(swapchain->imageCount);
    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(IndirectCount);
        indirectCountBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(IndirectCount);
        skeletalIndirectCountBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Model) * BINDLESS_MODEL_MATRIX_COUNT;
        modelBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Instance) * BINDLESS_INSTANCE_COUNT;
        instanceBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Model) * BINDLESS_MODEL_MATRIX_COUNT;
        jointMatrixBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));
    }
}

void RenderThread::InitializeResources()
{
    Utils::ScopedTimer timer{"[Render Thread] Resource Initialization"};

    DescriptorLayoutBuilder layoutBuilder{2};
    //
    {
        layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1); // Draw Image
        layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1); // Depth Image
        // Add render targets as needed
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        renderTargetSetLayout = VkResources::CreateDescriptorSetLayout(vulkanContext.get(), layoutCreateInfo);
        renderTargetDescriptors = DescriptorBufferStorageImage(vulkanContext.get(), renderTargetSetLayout.handle, 1);
        renderTargetDescriptors.AllocateDescriptorSet();
    }

    VkDescriptorImageInfo drawDescriptorInfo;
    drawDescriptorInfo.imageView = renderTargets->drawImageView.handle;
    drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    renderTargetDescriptors.UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);

    bindlessResourcesDescriptorBuffer = DescriptorBufferBindlessResources(vulkanContext.get());

    gradientComputePipeline = GradientComputePipeline(vulkanContext.get(), renderTargetSetLayout.handle);
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
            VkDescriptorImageInfo drawDescriptorInfo;
            drawDescriptorInfo.imageView = renderTargets->drawImageView.handle;
            drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            renderTargetDescriptors.UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);
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
    //auto timer = Utils::ScopedTimer(fmt::format("[Render Thread] Frame time (Frame {})", frameNumber));

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

    // Transition 1 - Prepare for compute draw
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL
        );
        auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // Draw compute
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientComputePipeline.gradientPipeline.handle);
        // Push Constants
        vkCmdPushConstants(cmd, gradientComputePipeline.gradientPipelineLayout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) * 2, scaledRenderExtent.data());
        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1]{};
        descriptorBufferBindingInfo[0] = renderTargetDescriptors.GetBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

        uint32_t bufferIndexImage = 0;
        VkDeviceSize bufferOffset = 0;
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientComputePipeline.gradientPipelineLayout.handle, 0, 1, &bufferIndexImage, &bufferOffset);
        uint32_t groupsX = (scaledRenderExtent[0] + 15) / 16;
        uint32_t groupsY = (scaledRenderExtent[1] + 15) / 16;
        vkCmdDispatch(cmd, groupsX, groupsY, 1);
    }

    // Transition 2 - Prepare for copy
    {
        VkImageMemoryBarrier2 barriers[2];
        barriers[0] = VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
        barriers[1] = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 2;
        depInfo.pImageMemoryBarriers = barriers;
        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    // Copy
    {
        VkOffset3D renderOffset = {static_cast<int32_t>(scaledRenderExtent[0]), static_cast<int32_t>(scaledRenderExtent[1]), 1};
        VkOffset3D swapchainOffset = {static_cast<int32_t>(swapchain->extent.width), static_cast<int32_t>(swapchain->extent.height), 1};
        VkImageBlit2 blitRegion{};
        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.srcOffsets[0] = {0, 0, 0};
        blitRegion.srcOffsets[1] = renderOffset;
        blitRegion.dstOffsets[0] = {0, 0, 0};
        blitRegion.dstOffsets[1] = swapchainOffset;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.srcImage = renderTargets->drawImage.handle;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImage = currentSwapchainImage;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;
        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(cmd, &blitInfo);
    }

    // Final transition
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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

    //LOG_INFO("[Render Thread] Processed frame {} (should match frame data's frame number - {})", frameNumber, engineMultithreading->frameBuffers[currentFrameInFlight].currentFrame);
    return RenderResponse::OK;
}

void RenderThread::UpdateFrameTimeStats(float frameTimeMs)
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

    if (frameNumber % 100 == 0 && frameNumber > 0) {
        float avg = frameTimeTracker.GetRollingAverage();
        LOG_INFO("[Render Thread] Rolling average frame time (last {} frames): {:.2f}ms ({:.1f} FPS)",
                 frameTimeTracker.GetSampleCount(), avg, 1000.0f / avg);
    }
}
} // Renderer
