//
// Created by William on 2025-10-09.
//

#include "renderer.h"

#include "swapchain.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"
#include "utils.h"
#include "VkBootstrap.h"
#include "vulkan_context.h"
#include "imgui_wrapper.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "input.h"
#include "vk_descriptors.h"
#include "vk_helpers.h"
#include "descriptor_buffer/descriptor_buffer_combined_image_sampler.h"
#include "descriptor_buffer/descriptor_buffer_storage_image.h"
#include "descriptor_buffer/descriptor_buffer_uniform.h"

namespace Renderer
{
Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Initialize()
{
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN; // | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Vulkan Test Bed",
        800,
        600,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    vulkanContext = std::make_unique<VulkanContext>(window);
    swapchain = std::make_unique<Swapchain>(vulkanContext.get());
    imgui = std::make_unique<ImguiWrapper>(vulkanContext.get(), window, swapchain->imageCount);

    frameSynchronization.resize(swapchain->imageCount);
    renderFramesInFlight = swapchain->imageCount;

    VkCommandPoolCreateInfo commandPoolCreateInfo = VkHelpers::CommandPoolCreateInfo();
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = VkHelpers::CommandBufferAllocateInfo(1);

    for (auto& frame : frameSynchronization) {
        VK_CHECK(vkCreateCommandPool(vulkanContext->device, &commandPoolCreateInfo, nullptr, &frame.commandPool));
        commandBufferAllocateInfo.commandPool = frame.commandPool;
        VK_CHECK(vkAllocateCommandBuffers(vulkanContext->device, &commandBufferAllocateInfo, &frame.commandBuffer));
    }

    // Sync Structures
    const VkFenceCreateInfo fenceCreateInfo = VkHelpers::FenceCreateInfo();
    const VkSemaphoreCreateInfo semaphoreCreateInfo = VkHelpers::SemaphoreCreateInfo();
    for (auto& frame : frameSynchronization) {
        VK_CHECK(vkCreateFence(vulkanContext->device, &fenceCreateInfo, nullptr, &frame.renderFence));
        VK_CHECK(vkCreateSemaphore(vulkanContext->device, &semaphoreCreateInfo, nullptr, &frame.swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(vulkanContext->device, &semaphoreCreateInfo, nullptr, &frame.renderSemaphore));
    }

    CreateResources();
}

void Renderer::CreateResources()
{
    DescriptorLayoutBuilder layoutBuilder{1};
    //
    {
        layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, BINDLESS_UNIFORM_BUFFER_COUNT);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        VK_CHECK(vkCreateDescriptorSetLayout(vulkanContext->device, &layoutCreateInfo, nullptr, &bindlessUniformSetLayout));
        bindlessUniforms = std::make_unique<DescriptorBufferUniform>(vulkanContext.get(), bindlessUniformSetLayout, renderFramesInFlight);
    }

    //
    {
        layoutBuilder.Clear();
        layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BINDLESS_COMBINED_IMAGE_SAMPLER_COUNT);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        VK_CHECK(vkCreateDescriptorSetLayout(vulkanContext->device, &layoutCreateInfo, nullptr, &bindlessCombinedImageSamplerSetLayout));
        bindlessCombinedImageSamplers = std::make_unique<DescriptorBufferCombinedImageSampler>(vulkanContext.get(), bindlessCombinedImageSamplerSetLayout, renderFramesInFlight);
    }

    //
    {
        layoutBuilder.Clear();
        layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, BINDLESS_STORAGE_IMAGE_COUNT);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        VK_CHECK(vkCreateDescriptorSetLayout(vulkanContext->device, &layoutCreateInfo, nullptr, &bindlessStorageImageSetLayout));
        bindlessStorageImages = std::make_unique<DescriptorBufferStorageImage>(vulkanContext.get(), bindlessStorageImageSetLayout, renderFramesInFlight);
        // Allocate all, we know they are all for FIF
        for (int32_t i = 0; i < renderFramesInFlight; ++i) {
            bindlessStorageImages->AllocateDescriptorSet();
        }
    }


    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageCreateInfo drawImageCreateInfo = VkHelpers::ImageCreateInfo(VK_FORMAT_R8G8B8A8_UNORM, {800, 600, 1}, drawImageUsages);
    drawImage = VkResources::CreateAllocatedImage(vulkanContext.get(), drawImageCreateInfo);

    VkImageViewCreateInfo drawImageViewCreateInfo = VkHelpers::ImageViewCreateInfo(drawImage.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    drawImageView = VkResources::CreateAllocatedImageView(vulkanContext.get(), drawImageViewCreateInfo);

    VkDescriptorImageInfo drawDescriptorInfo;
    drawDescriptorInfo.imageView = drawImageView.imageView;
    // Manually update all 3,
    for (int i = 0; i < renderFramesInFlight; ++i) {
        bindlessStorageImages->UpdateDescriptor(drawDescriptorInfo, i, 0);
    }

    VkCommandPool immediateCommandPool;
    VkCommandBuffer immediateCommandBuffer;
    VkFence immediateFence;

    const VkCommandPoolCreateInfo commandPoolCreateInfo = VkHelpers::CommandPoolCreateInfo();
    VK_CHECK(vkCreateCommandPool(vulkanContext->device, &commandPoolCreateInfo, nullptr, &immediateCommandPool));
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = VkHelpers::CommandBufferAllocateInfo(1, immediateCommandPool);
    VK_CHECK(vkAllocateCommandBuffers(vulkanContext->device, &commandBufferAllocateInfo, &immediateCommandBuffer));

    VkFenceCreateInfo fenceCreateInfo = VkHelpers::FenceCreateInfo();
    VK_CHECK(vkCreateFence(vulkanContext->device, &fenceCreateInfo, nullptr, &immediateFence));


    VK_CHECK(vkResetFences(vulkanContext->device, 1, &immediateFence));
    VK_CHECK(vkResetCommandBuffer(immediateCommandBuffer, 0));

    VkCommandBuffer cmd = immediateCommandBuffer;
    VkCommandBufferBeginInfo commandBufferBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    // Commands
    {}

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo commandBufferSubmitInfo = VkHelpers::CommandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&commandBufferSubmitInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, immediateFence));
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &immediateFence, true, 1000000000));

    vkDestroyCommandPool(vulkanContext->device, immediateCommandPool, nullptr);
    vkDestroyFence(vulkanContext->device, immediateFence, nullptr);
}

void Renderer::Run()
{
    Input& input = Input::get();
    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            input.processEvent(e);
            imgui->HandleInput(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        input.updateFocus(SDL_GetWindowFlags(window));

        if (exit) {
            bShouldExit = true;
            break;
        }

        Render();

        input.frameReset();
        frameNumber++;
    }
}

void Renderer::Render()
{
    Input& input = Input::get();

    const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
    const FrameData& currentFrameData = frameSynchronization[currentFrameInFlight];

    // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &currentFrameData.renderFence, true, 1000000000));
    // Un-signal fence, essentially saying "I'm using this frame-in-flight's resources, hands off".
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &currentFrameData.renderFence));


    // Do rendering stuff
    VkCommandBuffer cmd = currentFrameData.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (input.isKeyPressed(Key::G)) {
        LOG_INFO("G is pressed");
    }
    if (input.isKeyReleased(Key::G)) {
        LOG_INFO("G is released");
    }
    if (ImGui::Begin("Main")) {
        ImGui::Text("Hello!");
    }
    ImGui::End();
    ImGui::Render();


    uint32_t swapchainImageIndex;
    // (Non-Blocking) Acquire swapchain image index. Signal semaphore when the actual image is ready for use.
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, 1000000000, currentFrameData.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bWindowChanged = true;
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return;
    }

    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];
    VkImageView currentSwapchainImageView = swapchain->swapchainImageViews[swapchainImageIndex];


    // Transition 1
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // Clear Color
    {
        int32_t idiv = 360 * 1;
        float fdiv = 360 * 1;
        float t = (frameNumber % idiv) / fdiv;
        float r = std::sin(t * 6.28318f) * 0.5f + 0.5f;
        float g = std::sin((t + 0.333f) * 6.28318f) * 0.5f + 0.5f;
        float b = std::sin((t + 0.666f) * 6.28318f) * 0.5f + 0.5f;

        VkImageSubresourceRange range{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        VkClearColorValue clear = {r, g, b, 1.0f};
        vkCmdClearColorImage(cmd, currentSwapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    }

    // Transition 2
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );
        auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // Imgui Draw
    {
        VkRenderingAttachmentInfo imguiAttachment{};
        imguiAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        imguiAttachment.pNext = nullptr;
        imguiAttachment.imageView = currentSwapchainImageView;
        imguiAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imguiAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        imguiAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.pNext = nullptr;
        renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, swapchain->extent};
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &imguiAttachment;
        renderInfo.pDepthAttachment = nullptr;
        renderInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(cmd, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRendering(cmd);
    }


    // Transition 3
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
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
        bWindowChanged = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}

void Renderer::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    vkDestroyDescriptorSetLayout(vulkanContext->device, bindlessUniformSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, bindlessCombinedImageSamplerSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, bindlessStorageImageSetLayout, nullptr);

    drawImage.Cleanup(vulkanContext.get());
    drawImageView.Cleanup(vulkanContext.get());

    for (FrameData& frameData : frameSynchronization) {
        frameData.Cleanup(vulkanContext.get());
    }

    SDL_DestroyWindow(window);
}
}
