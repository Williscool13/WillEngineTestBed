//
// Created by William on 2025-11-05.
//

#include "staging_buffer.h"

#include "staging_uploader.h"
#include "core/time.h"
#include "crash-handling/crash_handler.h"
#include "fastgltf/types.hpp"
#include "input/input.h"
#include "render/render_utils.h"
#include "render/vk_descriptors.h"
#include "render/vk_helpers.h"
#include "render/vk_swapchain.h"
#include "render/model/model_loader.h"
#include "utils/utils.h"
#include "utils/world_constants.h"

StagingBuffer::StagingBuffer() {}

StagingBuffer::~StagingBuffer() {}

void StagingBuffer::Initialize()
{
    Utils::ScopedTimer timer{"Staging Buffer Initialization"};
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    renderContext = std::make_unique<Renderer::RenderContext>(Renderer::DEFAULT_SWAPCHAIN_WIDTH, Renderer::DEFAULT_SWAPCHAIN_HEIGHT, Renderer::DEFAULT_RENDER_SCALE);
    std::array<uint32_t, 2> renderExtent = renderContext->GetRenderExtent();

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow("Vulkan Test Bed", renderExtent[0], renderExtent[1], window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    vulkanContext = std::make_unique<Renderer::VulkanContext>(window);
    swapchain = std::make_unique<Renderer::Swapchain>(vulkanContext.get(), renderExtent[0], renderExtent[1]);
    imgui = std::make_unique<Renderer::ImguiWrapper>(vulkanContext.get(), window, swapchain->imageCount, swapchain->format);
    renderTargets = std::make_unique<Renderer::RenderTargets>(vulkanContext.get(), Renderer::DEFAULT_RENDER_TARGET_WIDTH, Renderer::DEFAULT_RENDER_TARGET_HEIGHT);
    Input::Input::Get().Init(window, swapchain->extent.width, swapchain->extent.height);

    renderFramesInFlight = swapchain->imageCount;

    frameSynchronization.reserve(renderFramesInFlight);
    for (int32_t i = 0; i < renderFramesInFlight; ++i) {
        frameSynchronization.emplace_back(vulkanContext.get());
        frameSynchronization[i].Initialize();
    }

    modelLoader = std::make_unique<Renderer::ModelLoader>(vulkanContext.get());
    stagingUploader = std::make_unique<StagingUploader>(vulkanContext.get());

    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
        VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.pNext = nullptr;
        VmaAllocationCreateInfo vmaAllocInfo = {};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Renderer::SceneData);
        sceneDataBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));
    }


    CreateResources();

    auto suzannePath = std::filesystem::path("../assets/Suzanne/glTF/Suzanne.gltf");
    LoadModelIntoBuffers(suzannePath, suzanneData);
    suzanneRuntimeMesh = GenerateModel(&suzanneData, Transform::Identity);
    UpdateTransforms(suzanneRuntimeMesh);
    InitialUploadRuntimeMesh(suzanneRuntimeMesh);
}

void StagingBuffer::CreateResources()
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
        renderTargetSetLayout = Renderer::VkResources::CreateDescriptorSetLayout(vulkanContext.get(), layoutCreateInfo);
        renderTargetDescriptors = Renderer::DescriptorBufferStorageImage(vulkanContext.get(), renderTargetSetLayout.handle, 1);
        renderTargetDescriptors.AllocateDescriptorSet();
    }

    VkDescriptorImageInfo drawDescriptorInfo;
    drawDescriptorInfo.imageView = renderTargets->drawImageView.handle;
    drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    renderTargetDescriptors.UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);

    bindlessResourcesDescriptorBuffer = Renderer::DescriptorBufferBindlessResources(vulkanContext.get());

    drawCullComputePipeline = Renderer::DrawCullComputePipeline(vulkanContext.get());
    mainRenderPipeline = Renderer::MainRenderPipeline(vulkanContext.get(), bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle);

    // Static data will be uploaded through a staging
    {
        // todo: swap around when ready to start staging
        VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.pNext = nullptr;
        VmaAllocationCreateInfo vmaAllocInfo = {};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vmaAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        // VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        // bufferInfo.pNext = nullptr;
        // VmaAllocationCreateInfo vmaAllocInfo = {};
        // vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        // vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT;
        megaVertexBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
        bufferInfo.usage = VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
        megaIndexBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(Renderer::MaterialProperties) * Renderer::MEGA_MATERIAL_BUFFER_COUNT;
        materialBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(fastgltf::Primitive) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT;
        primitiveBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    }


    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;
    bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * Renderer::BINDLESS_INSTANCE_COUNT;
    opaqueIndexedIndirectBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;
    bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * Renderer::BINDLESS_INSTANCE_COUNT;
    opaqueSkeletalIndexedIndirectBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    indirectCountBuffers.reserve(swapchain->imageCount);
    skeletalIndirectCountBuffers.reserve(swapchain->imageCount);
    modelBuffers.reserve(swapchain->imageCount);
    instanceBuffers.reserve(swapchain->imageCount);
    jointMatrixBuffers.reserve(swapchain->imageCount);
    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(Renderer::IndirectCount);
        indirectCountBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(Renderer::IndirectCount);
        skeletalIndirectCountBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Renderer::Model) * Renderer::BINDLESS_MODEL_MATRIX_COUNT;
        modelBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Renderer::Instance) * Renderer::BINDLESS_INSTANCE_COUNT;
        instanceBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Renderer::Model) * Renderer::BINDLESS_MODEL_MATRIX_COUNT;
        jointMatrixBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));
    }
}

void StagingBuffer::Run()
{
    Input& input = Input::Input::Get();
    Time& time = Time::Get();

    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            imgui->HandleInput(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bSwapchainOutdated = true;
            }
        }
        input.UpdateFocus(SDL_GetWindowFlags(window));
        time.Update();

        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            if (Renderer::RENDER_TARGET_SIZE_EQUALS_SWAPCHAIN_SIZE) {
                renderContext->RequestRenderExtentResize(w, h);
            }
            bSwapchainOutdated = false;
        }

        if (renderContext->HasPendingRenderExtentChanges()) {
            vkDeviceWaitIdle(vulkanContext->device);
            renderContext->ApplyRenderExtentResize();

            std::array<uint32_t, 2> newExtents = renderContext->GetRenderExtent();
            renderTargets->Recreate(newExtents[0], newExtents[1]);

            // Upload to descriptor buffer
            VkDescriptorImageInfo drawDescriptorInfo;
            drawDescriptorInfo.imageView = renderTargets->drawImageView.handle;
            drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            renderTargetDescriptors.UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);
        }


        if (exit) {
            bShouldExit = true;
            break;
        }

        Render();

        input.FrameReset();
        frameNumber++;
    }
}
void StagingBuffer::Render()
{
    std::array<uint32_t, 2> scaledRenderExtent = renderContext->GetScaledRenderExtent();
    Input& input = Input::Input::Get();
    const float deltaTime = Time::Get().GetDeltaTime();

    const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
    const Renderer::FrameSynchronization& currentFrameData = frameSynchronization[currentFrameInFlight];

    // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &currentFrameData.renderFence, true, 1000000000));
    // Un-signal fence, essentially saying "I'm using this frame-in-flight's resources, hands off" (when the command is submitted).
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &currentFrameData.renderFence));

    uint32_t swapchainImageIndex;
    // (Non-Blocking) Acquire swapchain image index. Signal semaphore when the actual image is ready for use.
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, 1000000000, currentFrameData.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return;
    }

    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];
    VkImageView currentSwapchainImageView = swapchain->swapchainImageViews[swapchainImageIndex];


    // Do rendering stuff
    VkCommandBuffer cmd = currentFrameData.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = Renderer::VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    constexpr float cameraPos[3] = {0, 0, -2};
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

    sceneData.deltaTime = deltaTime;

    Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
    Renderer::SceneData* currentSceneData = static_cast<Renderer::SceneData*>(currentSceneDataBuffer.allocationInfo.pMappedData);
    *currentSceneData = sceneData;

    //
    {
        // Draw/Cull pass (compute) - Construct indexed indirect buffer
        {
            {
                vkCmdFillBuffer(cmd, indirectCountBuffers[currentFrameInFlight].handle,offsetof(Renderer::IndirectCount, opaqueCount), sizeof(uint32_t), 0);
                vkCmdFillBuffer(cmd, skeletalIndirectCountBuffers[currentFrameInFlight].handle,offsetof(Renderer::IndirectCount, opaqueCount), sizeof(uint32_t), 0);
                VkBufferMemoryBarrier2 bufferBarrier[2];
                bufferBarrier[0] = Renderer::VkHelpers::BufferMemoryBarrier(
                    indirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
                bufferBarrier[1] = Renderer::VkHelpers::BufferMemoryBarrier(
                    skeletalIndirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

                VkDependencyInfo depInfo{};
                depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                depInfo.pNext = nullptr;
                depInfo.dependencyFlags = 0;
                depInfo.bufferMemoryBarrierCount = 2;
                depInfo.pBufferMemoryBarriers = bufferBarrier;

                vkCmdPipelineBarrier2(cmd, &depInfo);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, drawCullComputePipeline.drawCullPipeline.handle);
                Renderer::BindlessIndirectPushConstant pushData{
                    currentSceneDataBuffer.address,
                    primitiveBuffer.address,
                    modelBuffers[currentFrameInFlight].address,
                    instanceBuffers[currentFrameInFlight].address,
                    opaqueIndexedIndirectBuffer.address,
                    indirectCountBuffers[currentFrameInFlight].address,
                    opaqueSkeletalIndexedIndirectBuffer.address,
                    skeletalIndirectCountBuffers[currentFrameInFlight].address,
                };

                vkCmdPushConstants(cmd, drawCullComputePipeline.drawCullPipelineLayout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Renderer::BindlessIndirectPushConstant), &pushData);
                uint32_t groupsX = (highestInstanceIndex + 63) / 64;
                vkCmdDispatch(cmd, groupsX, 1, 1);
            }

            //
            {
                VkBufferMemoryBarrier2 bufferBarrier[4];
                bufferBarrier[0] = Renderer::VkHelpers::BufferMemoryBarrier(
                    opaqueIndexedIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
                bufferBarrier[1] = Renderer::VkHelpers::BufferMemoryBarrier(
                    indirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
                bufferBarrier[2] = Renderer::VkHelpers::BufferMemoryBarrier(
                    opaqueSkeletalIndexedIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
                bufferBarrier[3] = Renderer::VkHelpers::BufferMemoryBarrier(
                    skeletalIndirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
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
        }

        // Transition 1
        {
            auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
                renderTargets->drawImage.handle,
                subresource,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }

        // Draw render
        {
            constexpr VkClearValue colorClear = {.color = {0.0f, 1.0f, 0.0f, 1.0f}};
            const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
            const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
            const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({scaledRenderExtent[0], scaledRenderExtent[1]}, &colorAttachment, &depthAttachment);


            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPipeline.renderPipeline.handle);

            VkViewport viewport = Renderer::VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            Renderer::BindlessAddressPushConstant pushData{
                currentSceneDataBuffer.address,
                materialBuffer.address,
                primitiveBuffer.address,
                modelBuffers[currentFrameInFlight].address,
                instanceBuffers[currentFrameInFlight].address,
            };

            vkCmdPushConstants(cmd, mainRenderPipeline.renderPipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::BindlessAddressPushConstant), &pushData);

            VkDescriptorBufferBindingInfoEXT bindingInfo = bindlessResourcesDescriptorBuffer.GetBindingInfo();
            vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

            uint32_t bufferIndexImage = 0;
            VkDeviceSize bufferOffset = 0;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mainRenderPipeline.renderPipelineLayout.handle, 0, 1, &bufferIndexImage, &bufferOffset);


            const VkBuffer vertexBuffers[2] = {megaVertexBuffer.handle, megaVertexBuffer.handle};
            constexpr VkDeviceSize vertexOffsets[2] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexOffsets);
            vkCmdBindIndexBuffer(cmd, megaIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexedIndirectCount(cmd,
                                          opaqueIndexedIndirectBuffer.handle, 0,
                                          indirectCountBuffers[currentFrameInFlight].handle, offsetof(Renderer::IndirectCount, opaqueCount),
                                          highestInstanceIndex, sizeof(VkDrawIndexedIndirectCommand));

            vkCmdEndRendering(cmd);
        }

        // Transition 2 - Prepare for copy
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

        // Transition 3 - Prepare for imgui draw
        {
            auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
                currentSwapchainImage,
                subresource,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }
    }

    // Final transition -
    {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }


    VK_CHECK(vkEndCommandBuffer(cmd));


    VkCommandBufferSubmitInfo commandBufferSubmitInfo = Renderer::VkHelpers::CommandBufferSubmitInfo(currentFrameData.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(currentFrameData.swapchainSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
    VkSemaphoreSubmitInfo renderSemaphoreSignalInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(currentFrameData.renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = Renderer::VkHelpers::SubmitInfo(&commandBufferSubmitInfo, &swapchainSemaphoreWaitInfo, &renderSemaphoreSignalInfo);

    // Wait for swapchain semaphore, then submit command buffer. When finished, signal render semaphore and render fence.
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, currentFrameData.renderFence));

    // Wait for render semaphore, then present frame.
    VkPresentInfoKHR presentInfo = Renderer::VkHelpers::PresentInfo(&swapchain->handle, nullptr, &swapchainImageIndex);
    presentInfo.pWaitSemaphores = &currentFrameData.renderSemaphore;
    const VkResult presentResult = vkQueuePresentKHR(vulkanContext->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}
void StagingBuffer::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    SDL_DestroyWindow(window);
}

bool StagingBuffer::LoadModelIntoBuffers(const std::filesystem::path& modelPath, Renderer::ModelData& modelData)
{
    Renderer::ExtractedModel model = modelLoader->LoadGltf(modelPath);
    if (!model.bSuccessfullyLoaded) {
        return false;
    }

    modelData.name = modelPath.filename().string();
    modelData.path = modelPath;

    Renderer::AllocatedBuffer stagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{Renderer::STAGING_BUFFER_SIZE};
    // Vertices
    size_t sizeVertexPos = model.vertices.size() * sizeof(Renderer::Vertex);
    modelData.vertexAllocation = vertexBufferAllocator.allocate(sizeVertexPos);
    // if (modelData.vertexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
    //     LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in vertex buffer");
    //     return false;
    // }
    //memcpy(static_cast<char*>(megaVertexBuffer.allocationInfo.pMappedData) + modelData.vertexAllocation.offset, model.vertices.data(), sizeVertexPos);

    // Indices
    // todo: index upload
    size_t sizeIndices = model.indices.size() * sizeof(uint32_t);
    modelData.indexAllocation = indexBufferAllocator.allocate(sizeIndices);
    // if (modelData.indexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
    //     LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in index buffer");
    //     return false;
    // }
    // memcpy(static_cast<char*>(megaIndexBuffer.allocationInfo.pMappedData) + modelData.indexAllocation.offset, model.indices.data(), sizeIndices);

    // Descriptor assignment can happen here. Resource upload, will need to be staged and
    auto remapIndices = [](auto& indices, const std::vector<int32_t>& map) {
        indices.x = indices.x >= 0 ? map[indices.x] : -1;
        indices.y = indices.y >= 0 ? map[indices.y] : -1;
        indices.z = indices.z >= 0 ? map[indices.z] : -1;
        indices.w = indices.w >= 0 ? map[indices.w] : -1;
    };

    std::vector<int32_t> materialToBufferMap;

    // Samplers
    materialToBufferMap.resize(model.samplers.size());
    for (int32_t i = 0; i < model.samplers.size(); ++i) {
        materialToBufferMap[i] = bindlessResourcesDescriptorBuffer.AllocateSampler(model.samplers[i].handle);
    }

    for (Renderer::MaterialProperties& material : model.materials) {
        remapIndices(material.textureSamplerIndices, materialToBufferMap);
        remapIndices(material.textureSamplerIndices2, materialToBufferMap);
    }

    modelData.samplerIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);

    // Textures
    materialToBufferMap.clear();
    materialToBufferMap.resize(model.imageViews.size());

    for (int32_t i = 0; i < model.imageViews.size(); ++i) {
        materialToBufferMap[i] = bindlessResourcesDescriptorBuffer.AllocateTexture({
            .imageView = model.imageViews[i].handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    for (Renderer::MaterialProperties& material : model.materials) {
        remapIndices(material.textureImageIndices, materialToBufferMap);
        remapIndices(material.textureImageIndices2, materialToBufferMap);
    }

    modelData.textureIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);
    // Materials
    size_t sizeMaterials = model.materials.size() * sizeof(Renderer::MaterialProperties);
    modelData.materialAllocation = materialBufferAllocator.allocate(sizeMaterials);
    // memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData) + modelData.materialAllocation.offset, model.materials.data(), sizeMaterials);

    // Primitives
    uint32_t firstIndexCount = modelData.indexAllocation.offset / sizeof(uint32_t);
    uint32_t vertexOffsetCount = modelData.vertexAllocation.offset / sizeof(Renderer::Vertex);
    uint32_t materialOffsetCount = modelData.materialAllocation.offset / sizeof(Renderer::MaterialProperties);

    for (auto& primitive : model.primitives) {
        primitive.firstIndex += firstIndexCount;
        primitive.vertexOffset += static_cast<int32_t>(vertexOffsetCount);
        primitive.materialIndex += materialOffsetCount;
    }

    size_t sizePrimitives = model.primitives.size() * sizeof(fastgltf::Primitive);
    modelData.primitiveAllocation = primitiveBufferAllocator.allocate(sizePrimitives);
    // memcpy(static_cast<char*>(primitiveBuffer.allocationInfo.pMappedData) + modelData.primitiveAllocation.offset,
    //        model.primitives.data(), sizePrimitives);

    uint32_t primitiveOffsetCount = modelData.primitiveAllocation.offset / sizeof(fastgltf::Primitive);
    modelData.meshes = std::move(model.meshes);
    for (auto& mesh : modelData.meshes) {
        for (auto& primitiveIndex : mesh.primitiveIndices) {
            primitiveIndex += primitiveOffsetCount;
        }
    }

    stagingUploader->UploadStaticData(
        megaVertexBuffer, model.vertices, modelData.vertexAllocation,
        megaIndexBuffer, model.indices, modelData.indexAllocation,
        materialBuffer, model.materials, modelData.materialAllocation,
        primitiveBuffer, model.primitives, modelData.primitiveAllocation);


    modelData.samplers = std::move(model.samplers);
    modelData.images = std::move(model.images);
    modelData.imageViews = std::move(model.imageViews);
    modelData.nodes = std::move(model.nodes);

    modelData.inverseBindMatrices = std::move(model.inverseBindMatrices);
    modelData.animations = std::move(model.animations);
    modelData.nodeRemap = std::move(model.nodeRemap);

    return true;
}

RuntimeMesh StagingBuffer::GenerateModel(Renderer::ModelData* modelData, const Transform& topLevelTransform)
{
    RuntimeMesh rm{};

    rm.transform = topLevelTransform;
    rm.nodes.reserve(modelData->nodes.size());
    rm.nodeRemap = modelData->nodeRemap;

    size_t jointMatrixCount = modelData->inverseBindMatrices.size();
    bool bHasSkinning = jointMatrixCount > 0;
    if (bHasSkinning) {
        LOG_ERROR("No skinning for this test bed");
        exit(1);
    }

    rm.modelData = modelData;
    for (const Renderer::Node& n : modelData->nodes) {
        rm.nodes.emplace_back(n);
        Renderer::RuntimeNode& rn = rm.nodes.back();
        if (n.inverseBindIndex != ~0u) {
            rn.inverseBindMatrix = modelData->inverseBindMatrices[n.inverseBindIndex];
        }
    }


    return rm;
}

void StagingBuffer::UpdateTransforms(RuntimeMesh& runtimeMesh)
{
    glm::mat4 baseTopLevel = runtimeMesh.transform.GetMatrix();

    // Nodes are sorted
    for (Renderer::RuntimeNode& rn : runtimeMesh.nodes) {
        glm::mat4 localTransform = rn.transform.GetMatrix();

        if (rn.parent == ~0u) {
            rn.cachedWorldTransform = baseTopLevel * localTransform;
        }
        else {
            rn.cachedWorldTransform = runtimeMesh.nodes[rn.parent].cachedWorldTransform * localTransform;
        }
    }
}


void StagingBuffer::InitialUploadRuntimeMesh(RuntimeMesh& runtimeMesh)
{
    for (Renderer::RuntimeNode& node : runtimeMesh.nodes) {
        if (node.meshIndex != ~0u) {
            node.modelMatrixHandle = modelMatrixAllocator.Add();

            for (int32_t i = 0; i < swapchain->imageCount; ++i) {
                memcpy(
                    static_cast<char*>(modelBuffers[i].allocationInfo.pMappedData) + node.modelMatrixHandle.index * sizeof(Renderer::Model) + offsetof(Renderer::Model, modelMatrix),
                    &node.cachedWorldTransform,
                    sizeof(node.cachedWorldTransform));
            }

            if (runtimeMesh.modelData) {
                for (uint32_t primitiveIndex : runtimeMesh.modelData->meshes[node.meshIndex].primitiveIndices) {
                    Renderer::InstanceEntryHandle instanceEntry = instanceEntryAllocator.Add();
                    node.instanceEntryHandles.push_back(instanceEntry);

                    if ((instanceEntry.index + 1) > highestInstanceIndex) {
                        highestInstanceIndex = instanceEntry.index + 1;
                    }

                    Renderer::Instance inst;
                    inst.modelIndex = node.modelMatrixHandle.index;
                    inst.primitiveIndex = primitiveIndex;
                    inst.jointMatrixOffset = runtimeMesh.jointMatrixOffset;
                    inst.bIsAllocated = 1;
                    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
                        memcpy(static_cast<char*>(instanceBuffers[i].allocationInfo.pMappedData) + sizeof(Renderer::Instance) * instanceEntry.index, &inst, sizeof(Renderer::Instance));
                    }
                }
            }
        }

        if (node.jointMatrixIndex != ~0u) {
            LOG_ERROR("No skinning for this test bed");
            exit(1);
        }
    }
}