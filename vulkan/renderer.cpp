//
// Created by William on 2025-10-09.
//

#include "renderer.h"

#include "VkBootstrap.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_imgui_wrapper.h"
#include "render/vk_descriptors.h"
#include "render/vk_pipelines.h"
#include "render/vk_helpers.h"
#include "render/render_utils.h"
#include "render/render_constants.h"
#include "render/descriptor_buffer/descriptor_buffer_combined_image_sampler.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/descriptor_buffer/descriptor_buffer_uniform.h"

#include "input/input.h"

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
    imgui = std::make_unique<ImguiWrapper>(vulkanContext.get(), window, swapchain->imageCount, swapchain->format);

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
        VK_CHECK(vkCreateDescriptorSetLayout(vulkanContext->device, &layoutCreateInfo, nullptr, &renderTargetSetLayout));
        renderTargets = std::make_unique<DescriptorBufferStorageImage>(vulkanContext.get(), renderTargetSetLayout, 1);
        renderTargets->AllocateDescriptorSet();
    }
    //
    {
        layoutBuilder.Clear();
        layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, BINDLESS_UNIFORM_BUFFER_COUNT);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        VK_CHECK(vkCreateDescriptorSetLayout(vulkanContext->device, &layoutCreateInfo, nullptr, &bindlessUniformSetLayout));
        bindlessUniforms = std::make_unique<DescriptorBufferUniform>(vulkanContext.get(), bindlessUniformSetLayout, renderFramesInFlight);

        for (int32_t i = 0; i < renderFramesInFlight; ++i) {
            bindlessUniforms->AllocateDescriptorSet();
        }
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

        for (int32_t i = 0; i < renderFramesInFlight; ++i) {
            bindlessCombinedImageSamplers->AllocateDescriptorSet();
        }
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

    //
    {
        VkImageUsageFlags drawImageUsages{};
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo drawImageCreateInfo = VkHelpers::ImageCreateInfo(DRAW_IMAGE_FORMAT, {800, 600, 1}, drawImageUsages);
        drawImage = VkResources::CreateAllocatedImage(vulkanContext.get(), drawImageCreateInfo);

        VkImageViewCreateInfo drawImageViewCreateInfo = VkHelpers::ImageViewCreateInfo(drawImage.handle, DRAW_IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
        drawImageView = VkResources::CreateAllocatedImageView(vulkanContext.get(), drawImageViewCreateInfo);
    }

    //
    {
        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo depthImageCreateInfo = VkHelpers::ImageCreateInfo(DEPTH_IMAGE_FORMAT, {800, 600, 1}, depthImageUsages);
        depthImage = VkResources::CreateAllocatedImage(vulkanContext.get(), depthImageCreateInfo);

        VkImageViewCreateInfo depthImageViewCreateInfo = VkHelpers::ImageViewCreateInfo(depthImage.handle, DEPTH_IMAGE_FORMAT, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView = VkResources::CreateAllocatedImageView(vulkanContext.get(), depthImageViewCreateInfo);
    }

    VkDescriptorImageInfo drawDescriptorInfo;
    drawDescriptorInfo.imageView = drawImageView.handle;
    drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // only 1 set (invariant), binding 1 is the drawImage binding, not an array so 0
    renderTargets->UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);

    // Compute Pipeline
    {
        VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};
        computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        computePipelineLayoutCreateInfo.pNext = nullptr;
        computePipelineLayoutCreateInfo.pSetLayouts = &bindlessStorageImageSetLayout;
        computePipelineLayoutCreateInfo.setLayoutCount = 1;
        //  Push Constants
        /*VkPushConstantRange pushConstant{};
        pushConstant.offset = 0;
        pushConstant.size = sizeof(ComputePushConstants);
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;*/
        computePipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
        computePipelineLayoutCreateInfo.pushConstantRangeCount = 0;

        VK_CHECK(vkCreatePipelineLayout(vulkanContext->device, &computePipelineLayoutCreateInfo, nullptr, &computePipelineLayout));

        VkShaderModule computeShader;
        if (!VkHelpers::LoadShaderModule("shaders\\compute.comp.spv", vulkanContext->device, &computeShader)) {
            throw std::runtime_error("Error when building the compute shader (compute.comp.spv)");
        }

        VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = VkHelpers::PipelineShaderStageCreateInfo(computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
        VkComputePipelineCreateInfo computePipelineCreateInfo = VkHelpers::ComputePipelineCreateInfo(computePipelineLayout, pipelineShaderStageCreateInfo);
        VK_CHECK(vkCreateComputePipelines(vulkanContext->device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline));

        // Cleanup
        vkDestroyShaderModule(vulkanContext->device, computeShader, nullptr);
    }

    // Render Pipeline
    {
        VkPipelineLayoutCreateInfo renderPipelineLayoutCreateInfo{};
        renderPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        renderPipelineLayoutCreateInfo.setLayoutCount = 0;
        renderPipelineLayoutCreateInfo.pSetLayouts = nullptr;
        renderPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        renderPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
        // VkPushConstantRange renderPushConstantRange{};
        // renderPushConstantRange.offset = 0;
        // renderPushConstantRange.size = sizeof(RenderPushConstant);
        // renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        // std::array layouts{resourceManager.getSceneDataLayout(), samplerDescriptorLayout->layout};
        // layoutInfo.pSetLayouts = layouts.data();
        // layoutInfo.pPushConstantRanges = &renderPushConstantRange;
        // layoutInfo.pushConstantRangeCount = 1;

        VK_CHECK(vkCreatePipelineLayout(vulkanContext->device, &renderPipelineLayoutCreateInfo, nullptr, &renderPipelineLayout));

        VkShaderModule vertShader;
        VkShaderModule fragShader;
        if (!VkHelpers::LoadShaderModule("shaders\\vertex.vert.spv", vulkanContext->device, &vertShader)) {
            throw std::runtime_error("Error when building the compute shader (vertex.vert.spv)");
        }
        if (!VkHelpers::LoadShaderModule("shaders\\fragment.frag.spv", vulkanContext->device, &fragShader)) {
            throw std::runtime_error("Error when building the compute shader (fragment.frag.spv)");
        }

        RenderPipelineBuilder renderPipelineBuilder;
        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
        renderPipelineBuilder.disableMultisampling();
        renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        renderPipelineBuilder.setupRenderer({DRAW_IMAGE_FORMAT}, VK_FORMAT_D32_SFLOAT);
        renderPipelineBuilder.setupPipelineLayout(renderPipelineLayout);
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
        VK_CHECK(vkCreateGraphicsPipelines(vulkanContext->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &renderPipeline));

        vkDestroyShaderModule(vulkanContext->device, vertShader, nullptr);
        vkDestroyShaderModule(vulkanContext->device, fragShader, nullptr);
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
    Input::Input& input = Input::Input::Get();
    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            imgui->HandleInput(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        input.UpdateFocus(SDL_GetWindowFlags(window));

        if (exit) {
            bShouldExit = true;
            break;
        }

        Render();

        input.FrameReset();
        frameNumber++;
    }
}

void Renderer::Render()
{
    Input::Input& input = Input::Input::Get();

    const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
    const FrameData& currentFrameData = frameSynchronization[currentFrameInFlight];

    // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &currentFrameData.renderFence, true, 1000000000));
    // Un-signal fence, essentially saying "I'm using this frame-in-flight's resources, hands off".
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &currentFrameData.renderFence));

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


    // Do rendering stuff
    VkCommandBuffer cmd = currentFrameData.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    //
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (input.IsKeyPressed(Input::Key::G)) {
            LOG_INFO("G is pressed");
        }
        if (input.IsKeyReleased(Input::Key::G)) {
            LOG_INFO("G is released");
        }
        if (ImGui::Begin("Main")) {
            ImGui::Text("Hello!");
        }
        ImGui::End();
        ImGui::Render();
    }

    // Clear Color
    /*{
        // Transition 1
        {
            auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = VkHelpers::ImageMemoryBarrier(
                currentSwapchainImage,
                subresource,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
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
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }
    }*/

    // Compute Shader
    {
        // Transition 1 - Prepare for compute draw
        {
            auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = VkHelpers::ImageMemoryBarrier(
                drawImage.handle,
                subresource,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL
            );
            auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }

        // Draw compute
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            // Push Constants
            //vkCmdPushConstants(cmd, _backgroundEffectPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &selected._data);
            VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1]{};
            descriptorBufferBindingInfo[0] = renderTargets->GetBindingInfo();
            vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

            uint32_t bufferIndexImage = 0;
            VkDeviceSize bufferOffset = 0;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &bufferIndexImage, &bufferOffset);
            vkCmdDispatch(cmd, std::ceil(800 / 16.0), std::ceil(600 / 16.0), 1);
        }

        // Transition 2 - Prepare for render draw
        {
            auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = VkHelpers::ImageMemoryBarrier(
                drawImage.handle,
                subresource,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }

        // Draw render
        {
            const VkRenderingAttachmentInfo colorAttachment = VkHelpers::RenderingAttachmentInfo(drawImageView.handle, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
            const VkRenderingAttachmentInfo depthAttachment = VkHelpers::RenderingAttachmentInfo(depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
            const VkRenderingInfo renderInfo = VkHelpers::RenderingInfo({800, 600}, &colorAttachment, &depthAttachment);


            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline);

            // Dynamic States
            //  Viewport
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = 800;
            viewport.height = 600;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            //  Scissor
            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = 800;
            scissor.extent.height = 600;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdDraw(cmd, 6, 1, 0, 0);
            vkCmdEndRendering(cmd);
        }

        // Transition 2 - Prepare for copy
        {
            VkImageMemoryBarrier2 barriers[2];
            barriers[0] = VkHelpers::ImageMemoryBarrier(
                drawImage.handle,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            );
            barriers[1] = VkHelpers::ImageMemoryBarrier(
                currentSwapchainImage,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
            VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 2;
            depInfo.pImageMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(cmd, &depInfo);
        }

        // Copy
        {
            VkImageBlit2 blitRegion{};
            blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.srcOffsets[0] = {0,0,0};
            blitRegion.srcOffsets[1] = {800, 600, 1};
            blitRegion.dstOffsets[0] = {0,0,0};
            blitRegion.dstOffsets[1] = {800, 600, 1};

            VkBlitImageInfo2 blitInfo{};
            blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
            blitInfo.srcImage = drawImage.handle;
            blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitInfo.dstImage = currentSwapchainImage;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.regionCount = 1;
            blitInfo.pRegions = &blitRegion;
            blitInfo.filter = VK_FILTER_NEAREST;

            vkCmdBlitImage2(cmd, &blitInfo);
        }

        // Transition 3 - Prepare for imgui draw
        {
            auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = VkHelpers::ImageMemoryBarrier(
                currentSwapchainImage,
                subresource,
                VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }
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


    // Final transition -
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

    vkDestroyDescriptorSetLayout(vulkanContext->device, renderTargetSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, bindlessUniformSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, bindlessCombinedImageSamplerSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, bindlessStorageImageSetLayout, nullptr);

    vkDestroyPipelineLayout(vulkanContext->device, computePipelineLayout, nullptr);
    vkDestroyPipeline(vulkanContext->device, computePipeline, nullptr);
    vkDestroyPipelineLayout(vulkanContext->device, renderPipelineLayout, nullptr);
    vkDestroyPipeline(vulkanContext->device, renderPipeline, nullptr);

    drawImage.Cleanup(vulkanContext.get());
    drawImageView.Cleanup(vulkanContext.get());
    depthImage.Cleanup(vulkanContext.get());
    depthImageView.Cleanup(vulkanContext.get());

    for (FrameData& frameData : frameSynchronization) {
        frameData.Cleanup(vulkanContext.get());
    }

    SDL_DestroyWindow(window);
}
}
