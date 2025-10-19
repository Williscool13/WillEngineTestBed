//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_RENDERER_H
#define WILLENGINETESTBED_RENDERER_H

#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "render/render_context.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"


namespace Renderer
{
struct DescriptorBufferStorageImage;
struct DescriptorBufferCombinedImageSampler;
struct DescriptorBufferUniform;
class ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;

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
    std::unique_ptr<RenderTargets> renderTargets{};

    uint64_t frameNumber{0};
    std::vector<FrameData> frameSynchronization;
    int32_t renderFramesInFlight{0};

    // Probably want separate descriptor buffers/layouts for:
    //  - Scene Data
    VkDescriptorSetLayout renderTargetSetLayout{};
    std::unique_ptr<DescriptorBufferStorageImage> renderTargetDescriptors{};

    VkDescriptorSetLayout bindlessUniformSetLayout{};
    std::unique_ptr<DescriptorBufferUniform> bindlessUniforms{};
    VkDescriptorSetLayout bindlessCombinedImageSamplerSetLayout{};
    std::unique_ptr<DescriptorBufferCombinedImageSampler> bindlessCombinedImageSamplers{};
    VkDescriptorSetLayout bindlessStorageImageSetLayout{};
    std::unique_ptr<DescriptorBufferStorageImage> bindlessStorageImages{};



    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;

    VkPipelineLayout renderPipelineLayout;
    VkPipeline renderPipeline;


    bool bShouldExit{false};

    // Render Information
    bool bSwapchainOutdated{false};
    std::unique_ptr<RenderContext> renderContext{};

};
}


#endif //WILLENGINETESTBED_RENDERER_H
