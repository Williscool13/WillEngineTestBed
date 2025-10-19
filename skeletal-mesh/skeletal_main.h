//
// Created by William on 2025-10-18.
//

#ifndef WILLENGINETESTBED_SKELETAL_MESH_H
#define WILLENGINETESTBED_SKELETAL_MESH_H

#include <memory>
#include <vector>

#include <SDL3/SDL.h>
#include <volk/volk.h>

#include "render/vk_synchronization.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"

namespace Renderer
{
class ImguiWrapper;
struct Swapchain;
struct VulkanContext;

class SkeletalMain
{
public:
    SkeletalMain();

    ~SkeletalMain();

    void Initialize();

    void CreateResources();

    void Run();

    void Render();

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<VulkanContext> vulkanContext{};
    std::unique_ptr<Swapchain> swapchain{};
    std::unique_ptr<ImguiWrapper> imgui{};

    uint64_t frameNumber{0};
    std::vector<FrameData> frameSynchronization;
    int32_t renderFramesInFlight{0};

    // Probably want separate descriptor buffers/layouts for:
    //  - Scene Data
    VkDescriptorSetLayout renderTargetSetLayout;
    std::unique_ptr<DescriptorBufferStorageImage> renderTargets;

    // VkDescriptorSetLayout bindlessUniformSetLayout;
    // std::unique_ptr<DescriptorBufferUniform> bindlessUniforms;
    // VkDescriptorSetLayout bindlessCombinedImageSamplerSetLayout;
    // std::unique_ptr<DescriptorBufferCombinedImageSampler> bindlessCombinedImageSamplers;
    VkDescriptorSetLayout bindlessStorageImageSetLayout;
    std::unique_ptr<DescriptorBufferStorageImage> bindlessStorageImages;

    AllocatedImage drawImage;
    AllocatedImageView drawImageView;
    AllocatedImage depthImage;
    AllocatedImageView depthImageView;

    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;

    VkPipelineLayout renderPipelineLayout;
    VkPipeline renderPipeline;


    bool bShouldExit{false};
    bool bWindowChanged{false};
};
} // Renderer

#endif //WILLENGINETESTBED_SKELETAL_MESH_H