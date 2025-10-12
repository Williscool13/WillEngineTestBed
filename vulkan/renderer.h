//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_RENDERER_H
#define WILLENGINETESTBED_RENDERER_H

#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "synchronization.h"
#include "vk_resources.h"


namespace Renderer
{
struct DescriptorBufferStorageImage;
struct DescriptorBufferCombinedImageSampler;
struct DescriptorBufferUniform;
class ImguiWrapper;
struct VulkanContext;
struct Swapchain;

class Renderer
{
public:
    Renderer();

    ~Renderer();

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

    VkDescriptorSetLayout bindlessUniformSetLayout;
    std::unique_ptr<DescriptorBufferUniform> bindlessUniforms;
    VkDescriptorSetLayout bindlessCombinedImageSamplerSetLayout;
    std::unique_ptr<DescriptorBufferCombinedImageSampler> bindlessCombinedImageSamplers;
    VkDescriptorSetLayout bindlessStorageImageSetLayout;
    std::unique_ptr<DescriptorBufferStorageImage> bindlessStorageImages;

    AllocatedImage drawImage;
    AllocatedImageView drawImageView;

    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;


    bool bShouldExit{false};
    bool bWindowChanged{false};

};
}


#endif //WILLENGINETESTBED_RENDERER_H
