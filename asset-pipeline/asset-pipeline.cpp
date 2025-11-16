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

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_helpers.h"
#include "render/render_utils.h"

#include "input/input.h"
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
    Utils::ScopedTimer timer{"Template Initialization"};
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow(
        "Template",
        Core::DEFAULT_WINDOW_WIDTH,
        Core::DEFAULT_WINDOW_HEIGHT,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    Input::Input::Get().Init(window, w, h);

    vulkanContext = std::make_unique<Renderer::VulkanContext>(window);
    swapchain = std::make_unique<Renderer::Swapchain>(vulkanContext.get(), w, h);
    renderFramesInFlight = swapchain->imageCount;
    frameSynchronization.reserve(renderFramesInFlight);
    for (int32_t i = 0; i < renderFramesInFlight; ++i) {
        frameSynchronization.emplace_back(vulkanContext.get());
        frameSynchronization[i].Initialize();
    }

    bindlessResourcesDescriptorBuffer = Renderer::DescriptorBufferBindlessResources(vulkanContext.get());
    modelLoader = std::make_unique<Renderer::ModelLoader>(vulkanContext.get());

    CreateBuffers();

    CreateMeshletModel();
}

void AssetPipeline::Run()
{
    Input& input = Input::Input::Get();
    SDL_Event e;
    bool exit = false;
    while (true) {
        auto wait = std::chrono::milliseconds(8);
        std::this_thread::sleep_for(wait);

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
        input.UpdateFocus(SDL_GetWindowFlags(window));

        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            for (Renderer::FrameSynchronization& frameSync : frameSynchronization) {
                frameSync.RecreateSynchronization();
            }


            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            bSwapchainOutdated = false;
        }

        if (exit) {
            bShouldExit = true;
            break;
        }

        auto& currentFrameSync = frameSynchronization[frameNumber % renderFramesInFlight];
        Render(currentFrameSync);
        input.FrameReset();
        frameNumber++;
    }
}

void AssetPipeline::Render(Renderer::FrameSynchronization& frameSync)
{
    const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;

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

    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];

    VkCommandBuffer cmd = frameSync.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = Renderer::VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo)); {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    } {
        VkClearColorValue clear{0.3f, 0.0f, 0.0f, 1.0f};
        VkImageSubresourceRange subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkCmdClearColorImage(cmd, currentSwapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &subresource);
    } {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }


    VK_CHECK(vkEndCommandBuffer(cmd));


    VkCommandBufferSubmitInfo commandBufferSubmitInfo = Renderer::VkHelpers::CommandBufferSubmitInfo(frameSync.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(frameSync.swapchainSemaphore, VK_PIPELINE_STAGE_2_CLEAR_BIT);
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

    bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT;
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

    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
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

void AssetPipeline::CreateMeshletModel()
{
    auto bunnyPath = std::filesystem::path("../assets/stanford_bunny/stanford_bunny.gltf");
    Renderer::ExtractedMeshletModel meshletModel = modelLoader->LoadMeshletGltf(bunnyPath);
    if (!meshletModel.bSuccessfullyLoaded) {
        return;
    }

    meshletModelData.name = bunnyPath.filename().string();
    meshletModelData.path = bunnyPath;

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
    uint32_t primitiveOffset = meshletModelData.primitiveAllocation.offset / sizeof(Renderer::MeshletPrimitive);
    for (Renderer::Meshlet& meshlet : meshletModel.meshlets) {
        meshlet.vertexOffset += vertexOffset;
        meshlet.meshletVerticesOffset += meshletVerticesOffset;
        meshlet.meshletTriangleOffset += meshletTriangleOffset;
        meshlet.meshletPrimitiveIndex += primitiveOffset;
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


    meshletModelData.samplers = std::move(meshletModel.samplers);
    meshletModelData.images = std::move(meshletModel.images);
    meshletModelData.imageViews = std::move(meshletModel.imageViews);
    meshletModelData.nodes = std::move(meshletModel.nodes);

    meshletModelData.inverseBindMatrices = std::move(meshletModel.inverseBindMatrices);
    meshletModelData.animations = std::move(meshletModel.animations);
    meshletModelData.nodeRemap = std::move(meshletModel.nodeRemap);
}
}
