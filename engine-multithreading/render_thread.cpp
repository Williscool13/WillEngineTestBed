//
// Created by William on 2025-10-31.
//

#include "render_thread.h"

#include <VkBootstrap.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "engine_multithreading.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_imgui_wrapper.h"
#include "render/vk_descriptors.h"
#include "render/vk_helpers.h"
#include "render/render_context.h"
#include "render/render_utils.h"
#include "render/render_constants.h"
#include "render/render_targets.h"
#include "render/resource_manager.h"
#include "render/model/model_loader.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"

#include "input/input.h"
#include "core/time.h"
#include "utils/utils.h"

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
    window = window_;

    //
    {
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


        VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.pNext = nullptr;
        VmaAllocationCreateInfo vmaAllocInfo = {};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(SceneData);

        sceneDataBuffers.reserve(swapchain->imageCount);
        frameSynchronization.reserve(swapchain->imageCount);
        for (int32_t i = 0; i < swapchain->imageCount; ++i) {
            frameSynchronization.emplace_back(vulkanContext.get());
            frameSynchronization.back().Initialize();

            sceneDataBuffers.push_back(std::move(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo)));
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


    gradientComputePipeline = GradientComputePipeline(vulkanContext.get(), renderTargetSetLayout.handle);
    drawCullComputePipeline = DrawCullComputePipeline(vulkanContext.get());
    mainRenderPipeline = MainRenderPipeline(vulkanContext.get(), resourceManager->bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle);

    modelMatrixOperationRingBuffer.Initialize(FRAME_BUFFER_OPERATION_COUNT_LIMIT);
    instanceOperationRingBuffer.Initialize(FRAME_BUFFER_OPERATION_COUNT_LIMIT);
    jointMatrixOperationRingBuffer.Initialize(FRAME_BUFFER_OPERATION_COUNT_LIMIT);
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

        const uint32_t currentGameFrameInFlight = frameNumber % Core::FRAMES_IN_FLIGHT;
        const uint32_t currentRenderFrameInFlight = frameNumber % swapchain->imageCount;
        FrameBuffer& currentFrameBuffer = engineMultithreading->frameBuffers[currentGameFrameInFlight];
        FrameSynchronization& currentFrameSynchronization = frameSynchronization[currentRenderFrameInFlight];


        // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
        VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &currentFrameSynchronization.renderFence, true, 1000000000));

        auto frameStartTime = std::chrono::high_resolution_clock::now();

        ProcessOperations(currentFrameBuffer, currentRenderFrameInFlight);
        RenderResponse renderResponse = Render(currentRenderFrameInFlight, currentFrameSynchronization, currentFrameBuffer);
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

void RenderThread::ProcessOperations(FrameBuffer& currentFrameBuffer, uint32_t currentFrameInFlight)
{
    const AllocatedBuffer& currentModelBuffer = modelBuffers[currentFrameInFlight];
    const AllocatedBuffer& currentInstanceBuffer = instanceBuffers[currentFrameInFlight];
    const AllocatedBuffer& currentJointMatrixBuffers = jointMatrixBuffers[currentFrameInFlight];

    modelMatrixOperationRingBuffer.Enqueue(currentFrameBuffer.modelMatrixOperations);
    currentFrameBuffer.modelMatrixOperations.clear();
    modelMatrixOperationRingBuffer.ProcessOperations(static_cast<char*>(currentModelBuffer.allocationInfo.pMappedData), swapchain->imageCount + 1);

    instanceOperationRingBuffer.Enqueue(currentFrameBuffer.instanceOperations);
    currentFrameBuffer.instanceOperations.clear();
    instanceOperationRingBuffer.ProcessOperations(static_cast<char*>(currentInstanceBuffer.allocationInfo.pMappedData), swapchain->imageCount, highestInstanceIndex);

    jointMatrixOperationRingBuffer.Enqueue(currentFrameBuffer.jointMatrixOperations);
    currentFrameBuffer.jointMatrixOperations.clear();
    jointMatrixOperationRingBuffer.ProcessOperations(static_cast<char*>(currentJointMatrixBuffers.allocationInfo.pMappedData), swapchain->imageCount + 1);
}

RenderThread::RenderResponse RenderThread::Render(uint32_t currentRenderFrameInFlight, FrameSynchronization& currentFrameData, FrameBuffer& currentFrameBuffer)
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

    AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentRenderFrameInFlight];
    auto* currentSceneData = static_cast<SceneData*>(currentSceneDataBuffer.allocationInfo.pMappedData);
    float aspectRatio = renderContext->GetAspectRatio();
    glm::vec2 renderTargetSize = {scaledRenderExtent[0], scaledRenderExtent[1]};
    glm::vec2 texelSize = renderContext->GetTexelSize();
    ConstructSceneData(currentFrameBuffer.rawSceneData, *currentSceneData, aspectRatio, renderTargetSize, texelSize);

    //
    {
        vkCmdFillBuffer(cmd, indirectCountBuffers[currentRenderFrameInFlight].handle,offsetof(IndirectCount, opaqueCount), sizeof(uint32_t), 0);
        vkCmdFillBuffer(cmd, skeletalIndirectCountBuffers[currentRenderFrameInFlight].handle,offsetof(IndirectCount, opaqueCount), sizeof(uint32_t), 0);
        VkBufferMemoryBarrier2 bufferBarrier[2];
        bufferBarrier[0] = VkHelpers::BufferMemoryBarrier(
            indirectCountBuffers[currentRenderFrameInFlight].handle, 0, VK_WHOLE_SIZE,
            VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        bufferBarrier[1] = VkHelpers::BufferMemoryBarrier(
            skeletalIndirectCountBuffers[currentRenderFrameInFlight].handle, 0, VK_WHOLE_SIZE,
            VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.dependencyFlags = 0;
        depInfo.bufferMemoryBarrierCount = 2;
        depInfo.pBufferMemoryBarriers = bufferBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    //
    {
        if (highestInstanceIndex > 0) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, drawCullComputePipeline.drawCullPipeline.handle);
            BindlessIndirectPushConstant pushData{
                currentSceneDataBuffer.address,
                resourceManager->primitiveBuffer.address,
                modelBuffers[currentRenderFrameInFlight].address,
                instanceBuffers[currentRenderFrameInFlight].address,
                opaqueIndexedIndirectBuffer.address,
                indirectCountBuffers[currentRenderFrameInFlight].address,
                opaqueSkeletalIndexedIndirectBuffer.address,
                skeletalIndirectCountBuffers[currentRenderFrameInFlight].address,
            };

            vkCmdPushConstants(cmd, drawCullComputePipeline.drawCullPipelineLayout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BindlessIndirectPushConstant), &pushData);
            uint32_t groupsX = (highestInstanceIndex + 63) / 64;
            vkCmdDispatch(cmd, groupsX, 1, 1);
        }
    }

    //
    {
        VkBufferMemoryBarrier2 bufferBarrier[4];
        bufferBarrier[0] = VkHelpers::BufferMemoryBarrier(
            opaqueIndexedIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
        bufferBarrier[1] = VkHelpers::BufferMemoryBarrier(
            indirectCountBuffers[currentRenderFrameInFlight].handle, 0, VK_WHOLE_SIZE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
        bufferBarrier[2] = VkHelpers::BufferMemoryBarrier(
            opaqueSkeletalIndexedIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
        bufferBarrier[3] = VkHelpers::BufferMemoryBarrier(
            skeletalIndirectCountBuffers[currentRenderFrameInFlight].handle, 0, VK_WHOLE_SIZE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.dependencyFlags = 0;
        depInfo.bufferMemoryBarrierCount = 4;
        depInfo.pBufferMemoryBarriers = bufferBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    // Transition 1
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );
        auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }


    // Draw render
    {
        constexpr VkClearValue colorClear = {.color = {0.0f, 0.0f, 1.0f, 1.0f}};
        const VkRenderingAttachmentInfo colorAttachment = VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
        const VkRenderingAttachmentInfo depthAttachment = VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        const VkRenderingInfo renderInfo = VkHelpers::RenderingInfo({scaledRenderExtent[0], scaledRenderExtent[1]}, &colorAttachment, &depthAttachment);


        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPipeline.renderPipeline.handle);

        VkViewport viewport = VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor = VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        BindlessAddressPushConstant pushData{
            currentSceneDataBuffer.address,
            resourceManager->materialBuffer.address,
            resourceManager->primitiveBuffer.address,
            modelBuffers[currentRenderFrameInFlight].address,
            instanceBuffers[currentRenderFrameInFlight].address,
        };

        vkCmdPushConstants(cmd, mainRenderPipeline.renderPipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BindlessAddressPushConstant), &pushData);

        VkDescriptorBufferBindingInfoEXT bindingInfo = resourceManager->bindlessResourcesDescriptorBuffer.GetBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

        uint32_t bufferIndexImage = 0;
        VkDeviceSize bufferOffset = 0;
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPipeline.renderPipelineLayout.handle, 0, 1, &bufferIndexImage, &bufferOffset);


        const VkBuffer vertexBuffers[2] = {resourceManager->megaVertexBuffer.handle, resourceManager->megaVertexBuffer.handle};
        constexpr VkDeviceSize vertexOffsets[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexOffsets);
        vkCmdBindIndexBuffer(cmd, resourceManager->megaIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirectCount(cmd,
                                      opaqueIndexedIndirectBuffer.handle, 0,
                                      indirectCountBuffers[currentRenderFrameInFlight].handle, offsetof(IndirectCount, opaqueCount),
                                      highestInstanceIndex, sizeof(VkDrawIndexedIndirectCommand));

        vkCmdEndRendering(cmd);
    }

    // Transition 2 - Prepare for copy
    {
        VkImageMemoryBarrier2 barriers[2];
        barriers[0] = VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
        barriers[1] = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
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
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = VkHelpers::SemaphoreSubmitInfo(currentFrameData.swapchainSemaphore, VK_PIPELINE_STAGE_2_BLIT_BIT);
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

void RenderThread::ConstructSceneData(RawSceneData& raw, SceneData& scene, float aspectRatio, glm::vec2 renderTargetSize, glm::vec2 texelSize)
{
    scene.view = raw.view;
    scene.prevView = raw.prevView;
    scene.cameraWorldPos = {raw.cameraWorldPos, 0.0f};
    scene.prevCameraWorldPos = {raw.prevCameraWorldPos, 0.0f};
    scene.deltaTime = raw.deltaTime;

    scene.prevViewProj = scene.viewProj;
    scene.prevProj = scene.proj;
    scene.proj = glm::perspective(glm::radians(raw.fovDegrees), aspectRatio, raw.farPlane, raw.nearPlane);
    scene.viewProj = scene.proj * scene.view;

    scene.invView = glm::inverse(scene.view);
    scene.invProj = glm::inverse(scene.proj);
    scene.invViewProj = glm::inverse(scene.viewProj);
    scene.prevInvView = glm::inverse(scene.prevView);
    scene.prevInvProj = glm::inverse(scene.prevProj);
    scene.prevInvViewProj = glm::inverse(scene.prevViewProj);

    scene.viewProjCameraLookDirection = scene.proj * glm::mat4(glm::mat3(scene.view));
    scene.prevViewProjCameraLookDirection = scene.prevProj * glm::mat4(glm::mat3(scene.prevView));

    scene.renderTargetSize = renderTargetSize;
    scene.texelSize = glm::vec2(1.0f) / renderTargetSize;
    scene.cameraPlanes = glm::vec2(raw.farPlane, raw.nearPlane);
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

    if (frameNumber % 1000 == 0 && frameNumber > 0) {
        float avg = frameTimeTracker.GetRollingAverage();
        LOG_INFO("[Render Thread] Rolling average frame time (last {} frames): {:.2f}ms ({:.1f} FPS)",
                 frameTimeTracker.GetSampleCount(), avg, 1000.0f / avg);
    }
}
} // Renderer
