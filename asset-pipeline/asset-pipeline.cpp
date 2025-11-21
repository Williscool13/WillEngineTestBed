//
// Created by William on 2025-10-09.
//

#include "asset-pipeline.h"

#include <filesystem>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <VkBootstrap.h>
#include <backends/imgui_impl_vulkan.h>
#include <core/constants.h>

#include "meshoptimizer.h"
#include "core/time.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_helpers.h"
#include "render/render_utils.h"

#include "input/input.h"
#include "render/render_context.h"
#include "render/render_targets.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/model/model_loader.h"
#include "render/model/model_load_utils.h"
#include "utils/utils.h"

namespace AssetPipeline
{
AssetPipeline::AssetPipeline() = default;

AssetPipeline::~AssetPipeline() = default;

void AssetPipeline::Initialize()
{
    Utils::ScopedTimer timer{"Asset Pipeline Initialization"};
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    renderContext = std::make_unique<Renderer::RenderContext>(Renderer::DEFAULT_SWAPCHAIN_WIDTH, Renderer::DEFAULT_SWAPCHAIN_HEIGHT, Renderer::DEFAULT_RENDER_SCALE);
    std::array<uint32_t, 2> renderExtent = renderContext->GetRenderExtent();

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Asset Pipeline",
        renderExtent[0],
        renderExtent[1],
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    Input::Get().Init(window, w, h);

    renderContext->RequestRenderExtentResize(w, h);
    renderContext->ApplyRenderExtentResize();

    vulkanContext = std::make_unique<Renderer::VulkanContext>(window);
    swapchain = std::make_unique<Renderer::Swapchain>(vulkanContext.get(), w, h);
    renderTargets = std::make_unique<Renderer::RenderTargets>(vulkanContext.get(), Renderer::DEFAULT_RENDER_TARGET_WIDTH, Renderer::DEFAULT_RENDER_TARGET_HEIGHT);

    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::SceneData);
    // make 3 even though we may only get 2 from swapchain (optimize for production, simplify for test)
    constexpr int32_t tripleBuffering = 3;
    frameSynchronization.reserve(tripleBuffering);
    for (int32_t i = 0; i < tripleBuffering; ++i) {
        frameSynchronization.emplace_back(vulkanContext.get());
        frameSynchronization[i].Initialize();

        sceneDataBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));
    }

    modelLoader = std::make_unique<Renderer::ModelLoader>(vulkanContext.get());

    CreateBuffers();

    //CreateTrivialMeshletModel();

    CreateMeshletModel();

    meshShaderPipeline = Renderer::MainMeshShaderPipeline(vulkanContext.get(), bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle);
    meshDrawCullComputePipeline = Renderer::MeshDrawCullComputePipeline(vulkanContext.get());
    indirectMeshShaderPipeline = Renderer::IndirectMeshShaderPipeline(vulkanContext.get(), bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle);
}

void AssetPipeline::Run()
{
    Input& input = Input::Input::Get();
    Time& time = Time::Get();

    SDL_Event e;
    bool exit = false;
    while (true) {
        if (input.IsKeyPressed(Key::RETURN)) {
            Uint32 flags = SDL_GetWindowFlags(window);
            if (flags & SDL_WINDOW_BORDERLESS) {
                SDL_SetWindowFullscreen(window, false);
                SDL_SetWindowBordered(window, true);
                bSwapchainOutdated = true;
            }
            else {
                SDL_SetWindowBordered(window, false);
                SDL_SetWindowFullscreen(window, true);
                bSwapchainOutdated = true;
            }
        }

        input.FrameReset();
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
            if (e.type == SDL_EVENT_WINDOW_MINIMIZED
                || e.type == SDL_EVENT_WINDOW_RESTORED
                || e.type == SDL_EVENT_WINDOW_MAXIMIZED
                || e.type == SDL_EVENT_WINDOW_RESIZED
                || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bSwapchainOutdated = true;
            }
        }

        if (exit) {
            bShouldExit = true;
            break;
        }

        input.UpdateFocus(SDL_GetWindowFlags(window));
        time.Update();
        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            if (Renderer::RENDER_TARGET_SIZE_EQUALS_SWAPCHAIN_SIZE) { renderContext->RequestRenderExtentResize(w, h); }

            for (Renderer::FrameSynchronization& frameSync : frameSynchronization) {
                frameSync.RecreateSynchronization();
            }

            bSwapchainOutdated = false;
        }
        if (renderContext->HasPendingRenderExtentChanges()) {
            vkDeviceWaitIdle(vulkanContext->device);
            renderContext->ApplyRenderExtentResize();

            std::array<uint32_t, 2> newExtents = renderContext->GetRenderExtent();
            renderTargets->Recreate(newExtents[0], newExtents[1]);
        }


        const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
        auto& currentFrameSync = frameSynchronization[currentFrameInFlight];
        Render(currentFrameInFlight, currentFrameSync);
        frameNumber++;
    }
}

void AssetPipeline::Render(uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync)
{
    // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &frameSync.renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &frameSync.renderFence));

    uint32_t swapchainImageIndex;
    // Acquire swapchain image index. Signal semaphore when the actual image is ready for use.
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, 1000000000, frameSync.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return;
    }

    std::array<uint32_t, 2> scaledRenderExtent = renderContext->GetScaledRenderExtent();
    const Input& input = Input::Input::Get();
    const float deltaTime = Time::Get().GetDeltaTime();
    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];

    Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
    //
    {
        freeCamera.Update(deltaTime);
        const glm::vec3 cameraPos = freeCamera.GetPosition();
        const glm::vec3 forward = freeCamera.GetForward();
        const glm::vec3 up = freeCamera.GetUp();

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + forward, up);

        glm::mat4 proj = glm::perspective(
            freeCamera.GetFov(),
            static_cast<float>(scaledRenderExtent[0]) / static_cast<float>(scaledRenderExtent[1]),
            freeCamera.GetFarPlane(),
            freeCamera.GetNearPlane()
        );

        sceneData.view = view;
        sceneData.proj = proj;
        sceneData.viewProj = proj * view;
        sceneData.renderTargetSize.x = static_cast<float>(scaledRenderExtent[0]);
        sceneData.renderTargetSize.y = static_cast<float>(scaledRenderExtent[1]);
        sceneData.deltaTime = deltaTime;

        auto* currentSceneData = static_cast<Renderer::SceneData*>(currentSceneDataBuffer.allocationInfo.pMappedData);
        *currentSceneData = sceneData;
    }

    VkCommandBuffer cmd = frameSync.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = Renderer::VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));


    //
    {
        VkBufferMemoryBarrier2 bufferBarriers[2];
        bufferBarriers[0] = Renderer::VkHelpers::BufferMemoryBarrier(
            taskIndirectParameterBuffer.handle, 0, sizeof(uint32_t),
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        bufferBarriers[1] = Renderer::VkHelpers::BufferMemoryBarrier(
            taskIndirectParameterBuffer.handle, sizeof(glm::vec4), sizeof(Renderer::TaskIndirectDrawParameters) * Renderer::BINDLESS_INSTANCE_COUNT * 4,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.dependencyFlags = 0;
        depInfo.bufferMemoryBarrierCount = 2;
        depInfo.pBufferMemoryBarriers = bufferBarriers;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        vkCmdFillBuffer(cmd, taskIndirectParameterBuffer.handle, 0, sizeof(uint32_t), 0);

        VkBufferMemoryBarrier2 bufferBarrier = Renderer::VkHelpers::BufferMemoryBarrier(
            taskIndirectParameterBuffer.handle, 0, sizeof(uint32_t),
            VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

        depInfo.bufferMemoryBarrierCount = 1;
        depInfo.pBufferMemoryBarriers = &bufferBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, meshDrawCullComputePipeline.pipeline.handle);
        Renderer::MeshDrawCullComputePushConstant pushData{
            .sceneData = currentSceneDataBuffer.address,
            . primitiveBuffer = primitiveBuffer.address,
            .instanceBuffer = instanceBuffer.address,
            .taskIndirectParameterBuffer = taskIndirectParameterBuffer.address
        };

        vkCmdPushConstants(cmd, meshDrawCullComputePipeline.pipelineLayout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Renderer::MeshDrawCullComputePushConstant), &pushData);
        uint32_t groupsX = (1 + 63) / 64;
        vkCmdDispatch(cmd, groupsX, 1, 1);


        bufferBarrier = Renderer::VkHelpers::BufferMemoryBarrier(
            taskIndirectParameterBuffer.handle, 0, VK_WHOLE_SIZE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
        depInfo.pBufferMemoryBarriers = &bufferBarrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    // Prepare to render draw
    {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // Indirect Draw
    {
        constexpr VkClearValue colorClear = {.color = {0.0f, 0.0f, 1.0f, 1.0f}};
        const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
        const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({scaledRenderExtent[0], scaledRenderExtent[1]}, &colorAttachment, &depthAttachment);


        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, indirectMeshShaderPipeline.pipeline.handle);

        VkViewport viewport = Renderer::VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        Renderer::IndirectMeshShaderPushConstants pushData{
            .sceneData = currentSceneDataBuffer.address,
            .vertexBuffer = megaVertexBuffer.address,
            .meshletVerticesBuffer = megaMeshletVerticesBuffer.address,
            .meshletTrianglesBuffer = megaMeshletTrianglesBuffer.address,
            .meshletBuffer = megaMeshletBuffer.address,
            .meshIndirectParameterBuffer = taskIndirectParameterBuffer.address,
            .materialBuffer = materialBuffer.address,
            .modelBuffer = modelBuffer.address,
        };

        vkCmdPushConstants(cmd, indirectMeshShaderPipeline.pipelineLayout.handle, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(Renderer::IndirectMeshShaderPushConstants), &pushData);

        VkDescriptorBufferBindingInfoEXT bindingInfo = bindlessResourcesDescriptorBuffer.GetBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

        uint32_t bufferIndexImage = 0;
        VkDeviceSize bufferOffset = 0;
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, indirectMeshShaderPipeline.pipelineLayout.handle, 0, 1, &bufferIndexImage, &bufferOffset);

        vkCmdDrawMeshTasksIndirectCountEXT(cmd,
                                           taskIndirectParameterBuffer.handle, sizeof(glm::vec4),
                                           taskIndirectParameterBuffer.handle, 0,
                                           Renderer::BINDLESS_INSTANCE_COUNT, sizeof(glm::vec4) * 2);
        vkCmdEndRendering(cmd);
    }

    // Draw
    {
        // constexpr VkClearValue colorClear = {.color = {0.3f, 0.0f, 0.0f, 1.0f}};
        // const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
        // const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        // const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({scaledRenderExtent[0], scaledRenderExtent[1]}, &colorAttachment, &depthAttachment);
        //
        // vkCmdBeginRendering(cmd, &renderInfo);
        //
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshShaderPipeline.pipeline.handle);
        // //vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, basicMeshShaderPipeline.pipeline.handle);
        //
        // VkViewport viewport = Renderer::VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
        // vkCmdSetViewport(cmd, 0, 1, &viewport);
        // VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
        // vkCmdSetScissor(cmd, 0, 1, &scissor);
        //
        // Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
        // Renderer::MainMeshShaderPushConstants pushConstants{
        //     .sceneData = currentSceneDataBuffer.address,
        //     .vertexBuffer = megaVertexBuffer.address,
        //     .primitiveBuffer = primitiveBuffer.address,
        //     .meshletVerticesBuffer = megaMeshletVerticesBuffer.address,
        //     .meshletTrianglesBuffer = megaMeshletTrianglesBuffer.address,
        //     .meshletBuffer = megaMeshletBuffer.address,
        //     .materialBuffer = materialBuffer.address,
        //     .modelBuffer = modelBuffer.address,
        //     .instanceBuffer = instanceBuffer.address,
        //     .instanceIndex = 0
        // };
        // // Renderer::BasicMeshShaderPushConstants pushData{glm::mat4(1.0f),currentSceneDataBuffer.address,};
        //
        // vkCmdPushConstants(cmd, meshShaderPipeline.pipelineLayout.handle, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        //                    sizeof(Renderer::MainMeshShaderPushConstants), &pushConstants);
        // vkCmdDrawMeshTasksEXT(cmd, 1, 1, 1);
        //
        // pushConstants.instanceIndex = 1;
        // vkCmdPushConstants(cmd, meshShaderPipeline.pipelineLayout.handle, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        //                    sizeof(Renderer::MainMeshShaderPushConstants), &pushConstants);
        // //vkCmdPushConstants(cmd, basicMeshShaderPipeline.pipelineLayout.handle, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::BasicMeshShaderPushConstants), &pushData);
        //
        // vkCmdDrawMeshTasksEXT(cmd, 1, 1, 1);
        // vkCmdEndRendering(cmd);
    }

    // Prepare for copy
    {
        VkImageMemoryBarrier2 barriers[2];
        barriers[0] = Renderer::VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
        barriers[1] = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 2;
        depInfo.pImageMemoryBarriers = barriers;
        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    // Blit
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

    //
    {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo commandBufferSubmitInfo = Renderer::VkHelpers::CommandBufferSubmitInfo(frameSync.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(frameSync.swapchainSemaphore, VK_PIPELINE_STAGE_2_BLIT_BIT);
    VkSemaphoreSubmitInfo renderSemaphoreSignalInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(frameSync.renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = Renderer::VkHelpers::SubmitInfo(&commandBufferSubmitInfo, &swapchainSemaphoreWaitInfo, &renderSemaphoreSignalInfo);

    // Wait for swapchain semaphore, then submit command buffer. When finished, signal render semaphore and render fence.
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, frameSync.renderFence));

    // Wait for render semaphore, then present frame.
    VkPresentInfoKHR presentInfo = Renderer::VkHelpers::PresentInfo(&swapchain->handle, nullptr, &swapchainImageIndex);
    presentInfo.pWaitSemaphores = &frameSync.renderSemaphore;
    const VkResult presentResult = vkQueuePresentKHR(vulkanContext->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}

void AssetPipeline::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    SDL_DestroyWindow(window);
}

void AssetPipeline::CreateBuffers()
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT;
    megaVertexBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaMeshletVerticesBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(uint8_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaMeshletTrianglesBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaMeshletBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::MaterialProperties) * Renderer::MEGA_MATERIAL_BUFFER_COUNT;
    materialBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::Primitive) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT;
    primitiveBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::Model) * Renderer::BINDLESS_MODEL_MATRIX_COUNT;
    modelBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::Instance) * Renderer::BINDLESS_INSTANCE_COUNT;
    instanceBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    vmaAllocInfo.flags = 0;
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    bufferInfo.usage = VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    // vec4 for indirect count + padding. Instance_count * 4 is an assumption that each instance is likely to have at most 4 * 32 meshlets
    bufferInfo.size = sizeof(glm::vec4) + sizeof(Renderer::TaskIndirectDrawParameters) * Renderer::BINDLESS_INSTANCE_COUNT * 4;
    taskIndirectParameterBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    bindlessResourcesDescriptorBuffer = Renderer::DescriptorBufferBindlessResources(vulkanContext.get());
}

void AssetPipeline::CreateTrivialMeshletModel()
{
    // setup basic cube
    std::vector<Renderer::Vertex> cubeVertices = {
        // Front face (z+)
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        // Back face (z-)
        {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        // Top face (y+)
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        // Bottom face (y-)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
        // Right face (x+)
        {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
        // Left face (x-)
        {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    };
    std::vector<uint32_t> cubeIndices = {
        0, 1, 2, 2, 3, 0, // Front
        4, 5, 6, 6, 7, 4, // Back
        8, 9, 10, 10, 11, 8, // Top
        12, 13, 14, 14, 15, 12, // Bottom
        16, 17, 18, 18, 19, 16, // Right
        20, 21, 22, 22, 23, 20 // Left
    };

    const size_t max_vertices = 64;
    const size_t max_triangles = 64;
    const size_t target_group_size = 8;

    // build clusters (meshlets) out of the mesh
    size_t max_meshlets = meshopt_buildMeshletsBound(cubeIndices.size(), max_vertices, max_triangles);
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<unsigned int> meshletVertices(cubeIndices.size());
    std::vector<unsigned char> meshletTriangles(cubeIndices.size());

    std::vector<uint32_t> primitiveVertexPositions;
    meshlets.resize(meshopt_buildMeshlets(&meshlets[0], &meshletVertices[0], &meshletTriangles[0],
                                          cubeIndices.data(), cubeIndices.size(),
                                          reinterpret_cast<const float*>(cubeVertices.data()), cubeVertices.size(), sizeof(Renderer::Vertex),
                                          max_vertices, max_triangles, 0.f));

    // Optimize each meshlet's micro index buffer/vertex layout individually
    for (auto& meshlet : meshlets) {
        meshopt_optimizeMeshlet(&meshletVertices[meshlet.vertex_offset], &meshletTriangles[meshlet.triangle_offset], meshlet.triangle_count, meshlet.vertex_count);
    }

    // Trim the meshlet data to minimize waste for meshletVertices/meshletTriangles
    {
        // this is an example of how to trim the vertex/triangle arrays when copying data out to GPU storage
        const meshopt_Meshlet& last = meshlets.back();
        meshletVertices.resize(last.vertex_offset + last.vertex_count);
        meshletTriangles.resize(last.triangle_offset + last.triangle_count * 3);
    }


    // todo: meshlet bounding volume and cone
    Renderer::MeshletPrimitive primitiveData;
    primitiveData.meshletOffset = 0;
    primitiveData.meshletCount = meshlets.size();
    primitiveData.boundingSphere = Renderer::ModelLoadUtils::GenerateBoundingSphere(cubeVertices);

    // meshData.primitiveIndices.push_back(meshletModel.primitives.size());
    // meshletModel.primitives.push_back(primitiveData);

    uint32_t vertexOffset = 0;
    uint32_t meshletVertexOffset = 0;
    uint32_t meshletTrianglesOffset = 0;

    // meshletModel.vertices.insert(meshletModel.vertices.end(), primitiveVertices.begin(), primitiveVertices.end());
    // meshletModel.meshletVertices.insert(meshletModel.meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
    // meshletModel.meshletTriangles.insert(meshletModel.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());

    std::vector<Renderer::Meshlet> outputMeshlets; {};
    for (meshopt_Meshlet meshlet : meshlets) {
        outputMeshlets.push_back({
            .vertexOffset = vertexOffset,
            .meshletVerticesOffset = meshletVertexOffset + meshlet.vertex_offset,
            .meshletTriangleOffset = meshletTrianglesOffset + meshlet.triangle_offset,
            .meshletVerticesCount = meshlet.vertex_count,
            .meshletTriangleCount = meshlet.triangle_count,
        });
    }

    Renderer::MeshletModelData trivialMeshletModelData{};

    // Vertices
    size_t sizeVertices = cubeVertices.size() * sizeof(Renderer::Vertex);
    trivialMeshletModelData.vertexAllocation = vertexBufferAllocator.allocate(sizeVertices);
    if (trivialMeshletModelData.vertexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in vertex buffer");
        return;
    }
    memcpy(static_cast<char*>(megaVertexBuffer.allocationInfo.pMappedData) + trivialMeshletModelData.vertexAllocation.offset, cubeVertices.data(), sizeVertices);

    // Meshlet Vertices
    size_t sizeMeshletVertices = meshletVertices.size() * sizeof(uint32_t);
    trivialMeshletModelData.meshletVerticesAllocation = meshletVerticesBufferAllocator.allocate(sizeMeshletVertices);
    if (trivialMeshletModelData.meshletVerticesAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshletVertices buffer");
        return;
    }
    memcpy(static_cast<char*>(megaMeshletVerticesBuffer.allocationInfo.pMappedData) + trivialMeshletModelData.meshletVerticesAllocation.offset, meshletVertices.data(), sizeMeshletVertices);

    // Meshlet Triangles
    size_t sizeMeshletTriangles = meshletTriangles.size() * sizeof(uint8_t);
    trivialMeshletModelData.meshletTrianglesAllocation = meshletTrianglesBufferAllocator.allocate(sizeMeshletTriangles);
    if (trivialMeshletModelData.meshletTrianglesAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshletTriangles buffer");
        return;
    }
    memcpy(static_cast<char*>(megaMeshletTrianglesBuffer.allocationInfo.pMappedData) + trivialMeshletModelData.meshletTrianglesAllocation.offset, meshletTriangles.data(), sizeMeshletTriangles);

    // Meshlets
    uint32_t _vertexOffset = trivialMeshletModelData.vertexAllocation.offset / sizeof(Renderer::Vertex);
    uint32_t meshletVerticesOffset = trivialMeshletModelData.meshletVerticesAllocation.offset / sizeof(uint32_t);
    uint32_t meshletTriangleOffset = trivialMeshletModelData.meshletTrianglesAllocation.offset / sizeof(uint8_t);
    for (Renderer::Meshlet& meshlet : outputMeshlets) {
        meshlet.vertexOffset += _vertexOffset;
        meshlet.meshletVerticesOffset += meshletVerticesOffset;
        meshlet.meshletTriangleOffset += meshletTriangleOffset;
    }

    size_t sizeMeshlets = outputMeshlets.size() * sizeof(Renderer::Meshlet);
    trivialMeshletModelData.meshletAllocation = meshletBufferAllocator.allocate(sizeMeshlets);
    if (trivialMeshletModelData.meshletAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshlets buffer");
        return;
    }
    memcpy(static_cast<char*>(megaMeshletBuffer.allocationInfo.pMappedData) + trivialMeshletModelData.meshletAllocation.offset, outputMeshlets.data(), sizeMeshlets);


    // Primitives
    uint32_t meshletOffset = trivialMeshletModelData.meshletAllocation.offset / sizeof(Renderer::Meshlet);
    uint32_t materialOffsetCount = 0;

    primitiveData.meshletOffset += meshletOffset;
    primitiveData.materialIndex += materialOffsetCount;
    size_t sizePrimitives = 1 * sizeof(Renderer::MeshletPrimitive);
    trivialMeshletModelData.primitiveAllocation = primitiveBufferAllocator.allocate(sizePrimitives);
    if (trivialMeshletModelData.primitiveAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in primitives buffer");
        return;
    }
    memcpy(static_cast<char*>(primitiveBuffer.allocationInfo.pMappedData) + trivialMeshletModelData.primitiveAllocation.offset, &primitiveData, sizePrimitives);


    Renderer::Model modelMatrix = {1.0f};
    memcpy(static_cast<char*>(modelBuffer.allocationInfo.pMappedData), &modelMatrix, sizeof(Renderer::Model));

    // Add 1x instance (point to 1x model)
    Renderer::Instance i;
    i.modelIndex = 0;
    i.primitiveIndex = 0;
    i.bIsAllocated = 1;
    i.jointMatrixOffset = 0;
    memcpy(static_cast<char*>(instanceBuffer.allocationInfo.pMappedData), &i, sizeof(Renderer::Instance));
}

void AssetPipeline::CreateMeshletModel()
{
    auto bunnyPath = std::filesystem::path("../assets/stanford_bunny/stanford_bunny.gltf");
    Renderer::ExtractedMeshletModel meshletModel = modelLoader->LoadMeshletGltf(bunnyPath);
    if (!meshletModel.bSuccessfullyLoaded) {
        return;
    }

    meshletModelData.name = bunnyPath.filename().string();
    meshletModelData.path = bunnyPath;

    // Descriptor assignment can happen here. Resource upload, will need to be staged and
    auto remapIndices = [](auto& indices, const std::vector<int32_t>& map) {
        indices.x = indices.x >= 0 ? map[indices.x] : -1;
        indices.y = indices.y >= 0 ? map[indices.y] : -1;
        indices.z = indices.z >= 0 ? map[indices.z] : -1;
        indices.w = indices.w >= 0 ? map[indices.w] : -1;
    };

    std::vector<int32_t> materialToBufferMap;

    // Samplers
    materialToBufferMap.resize(meshletModel.samplers.size());
    for (int32_t i = 0; i < meshletModel.samplers.size(); ++i) {
        materialToBufferMap[i] = bindlessResourcesDescriptorBuffer.AllocateSampler(meshletModel.samplers[i].handle);
    }

    for (Renderer::MaterialProperties& material : meshletModel.materials) {
        remapIndices(material.textureSamplerIndices, materialToBufferMap);
        remapIndices(material.textureSamplerIndices2, materialToBufferMap);
    }

    meshletModelData.samplerIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);

    // Textures
    materialToBufferMap.clear();
    materialToBufferMap.resize(meshletModel.imageViews.size());

    for (int32_t i = 0; i < meshletModel.imageViews.size(); ++i) {
        materialToBufferMap[i] = bindlessResourcesDescriptorBuffer.AllocateTexture({
            .imageView = meshletModel.imageViews[i].handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    for (Renderer::MaterialProperties& material : meshletModel.materials) {
        remapIndices(material.textureImageIndices, materialToBufferMap);
        remapIndices(material.textureImageIndices2, materialToBufferMap);
    }

    meshletModelData.textureIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);
    // Materials
    size_t sizeMaterials = meshletModel.materials.size() * sizeof(Renderer::MaterialProperties);
    meshletModelData.materialAllocation = materialBufferAllocator.allocate(sizeMaterials);
    memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData) + meshletModelData.materialAllocation.offset, meshletModel.materials.data(), sizeMaterials);


    // Vertices
    size_t sizeVertices = meshletModel.vertices.size() * sizeof(Renderer::Vertex);
    meshletModelData.vertexAllocation = vertexBufferAllocator.allocate(sizeVertices);
    if (meshletModelData.vertexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in vertex buffer");
        return;
    }
    memcpy(static_cast<char*>(megaVertexBuffer.allocationInfo.pMappedData) + meshletModelData.vertexAllocation.offset, meshletModel.vertices.data(), sizeVertices);

    // Meshlet Vertices
    size_t sizeMeshletVertices = meshletModel.meshletVertices.size() * sizeof(uint32_t);
    meshletModelData.meshletVerticesAllocation = meshletVerticesBufferAllocator.allocate(sizeMeshletVertices);
    if (meshletModelData.meshletVerticesAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshletVertices buffer");
        return;
    }
    memcpy(static_cast<char*>(megaMeshletVerticesBuffer.allocationInfo.pMappedData) + meshletModelData.meshletVerticesAllocation.offset, meshletModel.meshletVertices.data(), sizeMeshletVertices);

    // Meshlet Triangles
    size_t sizeMeshletTriangles = meshletModel.meshletTriangles.size() * sizeof(uint8_t);
    meshletModelData.meshletTrianglesAllocation = meshletTrianglesBufferAllocator.allocate(sizeMeshletTriangles);
    if (meshletModelData.meshletTrianglesAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshletTriangles buffer");
        return;
    }
    memcpy(static_cast<char*>(megaMeshletTrianglesBuffer.allocationInfo.pMappedData) + meshletModelData.meshletTrianglesAllocation.offset, meshletModel.meshletTriangles.data(), sizeMeshletTriangles);

    // Meshlets
    uint32_t vertexOffset = meshletModelData.vertexAllocation.offset / sizeof(Renderer::Vertex);
    uint32_t meshletVerticesOffset = meshletModelData.meshletVerticesAllocation.offset / sizeof(uint32_t);
    uint32_t meshletTriangleOffset = meshletModelData.meshletTrianglesAllocation.offset / sizeof(uint8_t);
    for (Renderer::Meshlet& meshlet : meshletModel.meshlets) {
        meshlet.vertexOffset += vertexOffset;
        meshlet.meshletVerticesOffset += meshletVerticesOffset;
        meshlet.meshletTriangleOffset += meshletTriangleOffset;
    }

    size_t sizeMeshlets = meshletModel.meshlets.size() * sizeof(Renderer::Meshlet);
    meshletModelData.meshletAllocation = meshletBufferAllocator.allocate(sizeMeshlets);
    if (meshletModelData.meshletAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshlets buffer");
        return;
    }
    memcpy(static_cast<char*>(megaMeshletBuffer.allocationInfo.pMappedData) + meshletModelData.meshletAllocation.offset, meshletModel.meshlets.data(), sizeMeshlets);


    // Primitives
    uint32_t meshletOffset = meshletModelData.meshletAllocation.offset / sizeof(Renderer::Meshlet);
    uint32_t materialOffsetCount = meshletModelData.materialAllocation.offset / sizeof(Renderer::MaterialProperties);
    for (auto& primitive : meshletModel.primitives) {
        primitive.meshletOffset += meshletOffset;
        primitive.materialIndex += materialOffsetCount;
    }

    size_t sizePrimitives = meshletModel.primitives.size() * sizeof(Renderer::MeshletPrimitive);
    meshletModelData.primitiveAllocation = primitiveBufferAllocator.allocate(sizePrimitives);
    if (meshletModelData.primitiveAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in primitives buffer");
        return;
    }
    memcpy(static_cast<char*>(primitiveBuffer.allocationInfo.pMappedData) + meshletModelData.primitiveAllocation.offset, meshletModel.primitives.data(), sizePrimitives);

    meshletModelData.samplers = std::move(meshletModel.samplers);
    meshletModelData.images = std::move(meshletModel.images);
    meshletModelData.imageViews = std::move(meshletModel.imageViews);
    meshletModelData.nodes = std::move(meshletModel.nodes);

    meshletModelData.inverseBindMatrices = std::move(meshletModel.inverseBindMatrices);
    meshletModelData.animations = std::move(meshletModel.animations);
    meshletModelData.nodeRemap = std::move(meshletModel.nodeRemap);


    glm::mat4 mat{1.0f};
    mat = glm::translate(mat, glm::vec3(0.0f, 0.0f, 0.0f));
    Renderer::Model modelMatrix{mat};
    memcpy(static_cast<char*>(modelBuffer.allocationInfo.pMappedData), &modelMatrix, sizeof(Renderer::Model));

    glm::mat4 mat2{1.0f};
    mat2 = glm::translate(mat2, glm::vec3(2.0f, 2.0f, 0.0f));
    Renderer::Model modelMatrix2{mat2};
    memcpy(static_cast<char*>(modelBuffer.allocationInfo.pMappedData) + sizeof(Renderer::Model), &modelMatrix2, sizeof(Renderer::Model));

    // Add 1x instance (point to 1x model)
    Renderer::Instance i;
    i.modelIndex = 0;
    i.primitiveIndex = 0;
    i.bIsAllocated = 1;
    i.jointMatrixOffset = 0;
    Renderer::Instance i2;
    i2.modelIndex = 1;
    i2.primitiveIndex = 0;
    i2.bIsAllocated = 1;
    i2.jointMatrixOffset = 0;

    Renderer::Instance dummyInst{0, 0, 0, false};
    memcpy(static_cast<char*>(instanceBuffer.allocationInfo.pMappedData), &i, sizeof(Renderer::Instance));
    memcpy(static_cast<char*>(instanceBuffer.allocationInfo.pMappedData) + sizeof(Renderer::Instance), &i2, sizeof(Renderer::Instance));
    auto a = static_cast<Renderer::Instance*>(instanceBuffer.allocationInfo.pMappedData)[0];
    auto b = static_cast<Renderer::Instance*>(instanceBuffer.allocationInfo.pMappedData)[0];
    auto c = static_cast<Renderer::Instance*>(instanceBuffer.allocationInfo.pMappedData)[2];
    auto d = static_cast<Renderer::Instance*>(instanceBuffer.allocationInfo.pMappedData)[3];
    //memcpy(static_cast<char*>(instanceBuffer.allocationInfo.pMappedData), &i, sizeof(Renderer::Instance));
}
}
