//
// Created by William on 2025-10-09.
//

#include "meshlet-rendering.h"

#include <filesystem>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <VkBootstrap.h>
#include <backends/imgui_impl_vulkan.h>

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

namespace MeshletRendering
{
MeshletRendering::MeshletRendering() = default;

MeshletRendering::~MeshletRendering() = default;

void MeshletRendering::Initialize()
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

    meshDrawCullComputePipeline = Renderer::MeshDrawCullComputePipeline(vulkanContext.get());
    indirectMeshShaderPipeline = Renderer::IndirectMeshShaderPipeline(vulkanContext.get(), bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle);

    auto bunnyPath = std::filesystem::path("../assets/stanford_bunny/stanford_bunny.gltf");
    auto dragonPath = std::filesystem::path("../assets/dragon/dragon.gltf");
    bunnyModel = CreateMeshletModel(bunnyPath);
    dragonModel = CreateMeshletModel(dragonPath);


    std::array<Renderer::Model, 10> modelMatrices{};
    float spacing = 3.0f;
    float rowSpacing = 3.0f;

    for (int i = 0; i < 5; i++) {
        // Dragon in row 0
        glm::mat4 dragonMat{1.0f};
        dragonMat = glm::translate(dragonMat, glm::vec3(i * spacing, 0.0f, 0.0f));
        modelMatrices[i] = Renderer::Model{dragonMat};

        // Bunny in row 1
        glm::mat4 bunnyMat{1.0f};
        bunnyMat = glm::translate(bunnyMat, glm::vec3(i * spacing, rowSpacing, 0.0f));
        modelMatrices[i + 5] = Renderer::Model{bunnyMat};
    }

    memcpy(modelBuffer.allocationInfo.pMappedData, modelMatrices.data(), sizeof(Renderer::Model) * 10);

    std::array<Renderer::Instance, 10> instances{};

    for (int i = 0; i < 5; i++) {
        // Dragon instances
        instances[i].modelIndex = i;
        instances[i].primitiveIndex = 0;
        instances[i].bIsAllocated = 1;
        instances[i].jointMatrixOffset = 0;

        // Bunny instances
        instances[i + 5].modelIndex = i + 5;
        instances[i + 5].primitiveIndex = 1;
        instances[i + 5].bIsAllocated = 1;
        instances[i + 5].jointMatrixOffset = 0;
    }

    memcpy(instanceBuffer.allocationInfo.pMappedData, instances.data(), sizeof(Renderer::Instance) * 10);
}

void MeshletRendering::Run()
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

void MeshletRendering::Render(uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync)
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
        sceneData.frustum = Renderer::Frustum(sceneData.viewProj);
        sceneData.cameraWorldPos = {freeCamera.transform.translation, 0.0f};
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


    // Compute
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
        constexpr VkClearValue colorClear = {.color = {1.0f, 1.0f, 1.0f, 1.0f}};
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

void MeshletRendering::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    SDL_DestroyWindow(window);
}

void MeshletRendering::CreateBuffers()
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
    bufferInfo.size = Renderer::MEGA_MESHLET_VERTEX_BUFFER_SIZE;
    megaMeshletVerticesBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = Renderer::MEGA_MESHLET_TRIANGLE_BUFFER_SIZE;
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

Renderer::MeshletModelData MeshletRendering::CreateMeshletModel(const std::filesystem::path& path)
{
    Renderer::MeshletModelData model;
    Renderer::ExtractedMeshletModel meshletModel = modelLoader->LoadMeshletGltf(path);
    if (!meshletModel.bSuccessfullyLoaded) {
        return {};
    }

    model.name = path.filename().string();
    model.path = path;

    // Materials
    {
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

        model.samplerIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);

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

        model.textureIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);
        // Materials
        size_t sizeMaterials = meshletModel.materials.size() * sizeof(Renderer::MaterialProperties);
        model.materialAllocation = materialBufferAllocator.allocate(sizeMaterials);
        memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData) + model.materialAllocation.offset, meshletModel.materials.data(), sizeMaterials);
    }

    // Vertices
    size_t sizeVertices = meshletModel.vertices.size() * sizeof(Renderer::Vertex);
    model.vertexAllocation = vertexBufferAllocator.allocate(sizeVertices);
    if (model.vertexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in vertex buffer");
        return {};
    }
    memcpy(static_cast<char*>(megaVertexBuffer.allocationInfo.pMappedData) + model.vertexAllocation.offset, meshletModel.vertices.data(), sizeVertices);

    // Meshlet Vertices
    size_t sizeMeshletVertices = meshletModel.meshletVertices.size() * sizeof(uint32_t);
    model.meshletVerticesAllocation = meshletVerticesBufferAllocator.allocate(sizeMeshletVertices);
    if (model.meshletVerticesAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshletVertices buffer");
        return {};
    }
    memcpy(static_cast<char*>(megaMeshletVerticesBuffer.allocationInfo.pMappedData) + model.meshletVerticesAllocation.offset, meshletModel.meshletVertices.data(), sizeMeshletVertices);

    // Meshlet Triangles
    size_t sizeMeshletTriangles = meshletModel.meshletTriangles.size() * sizeof(uint8_t);
    model.meshletTrianglesAllocation = meshletTrianglesBufferAllocator.allocate(sizeMeshletTriangles);
    if (model.meshletTrianglesAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshletTriangles buffer");
        return {};
    }
    memcpy(static_cast<char*>(megaMeshletTrianglesBuffer.allocationInfo.pMappedData) + model.meshletTrianglesAllocation.offset, meshletModel.meshletTriangles.data(), sizeMeshletTriangles);

    // Meshlets
    uint32_t vertexOffset = model.vertexAllocation.offset / sizeof(Renderer::Vertex);
    uint32_t meshletVerticesOffset = model.meshletVerticesAllocation.offset / sizeof(uint32_t);
    uint32_t meshletTriangleOffset = model.meshletTrianglesAllocation.offset / sizeof(uint8_t);
    for (Renderer::Meshlet& meshlet : meshletModel.meshlets) {
        meshlet.vertexOffset += vertexOffset;
        meshlet.meshletVerticesOffset += meshletVerticesOffset;
        meshlet.meshletTriangleOffset += meshletTriangleOffset;
    }

    size_t sizeMeshlets = meshletModel.meshlets.size() * sizeof(Renderer::Meshlet);
    model.meshletAllocation = meshletBufferAllocator.allocate(sizeMeshlets);
    if (model.meshletAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in meshlets buffer");
        return {};
    }
    memcpy(static_cast<char*>(megaMeshletBuffer.allocationInfo.pMappedData) + model.meshletAllocation.offset, meshletModel.meshlets.data(), sizeMeshlets);


    // Primitives
    uint32_t meshletOffset = model.meshletAllocation.offset / sizeof(Renderer::Meshlet);
    uint32_t materialOffsetCount = model.materialAllocation.offset / sizeof(Renderer::MaterialProperties);
    for (auto& primitive : meshletModel.primitives) {
        primitive.meshletOffset += meshletOffset;
        primitive.materialIndex += materialOffsetCount;
    }

    size_t sizePrimitives = meshletModel.primitives.size() * sizeof(Renderer::MeshletPrimitive);
    model.primitiveAllocation = primitiveBufferAllocator.allocate(sizePrimitives);
    if (model.primitiveAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in primitives buffer");
        return {};
    }
    memcpy(static_cast<char*>(primitiveBuffer.allocationInfo.pMappedData) + model.primitiveAllocation.offset, meshletModel.primitives.data(), sizePrimitives);

    model.samplers = std::move(meshletModel.samplers);
    model.images = std::move(meshletModel.images);
    model.imageViews = std::move(meshletModel.imageViews);
    model.nodes = std::move(meshletModel.nodes);

    model.inverseBindMatrices = std::move(meshletModel.inverseBindMatrices);
    model.animations = std::move(meshletModel.animations);
    model.nodeRemap = std::move(meshletModel.nodeRemap);

    return model;
}
}
