//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_LOADING_H
#define WILLENGINETESTBED_MODEL_LOADING_H
#include <memory>
#include <vector>

#include "offsetAllocator.hpp"
#include "SDL3/SDL.h"

#include "render/render_context.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/model/model_data.h"

namespace Renderer
{
class ModelLoader;
struct DescriptorBufferStorageImage;
struct DescriptorBufferCombinedImageSampler;
struct DescriptorBufferUniform;
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;


class ModelLoading
{
public:
    ModelLoading();

    ~ModelLoading();

    void CreateResources();

    void Initialize();

    void Run();

    void Render();

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<VulkanContext> vulkanContext{};
    std::unique_ptr<Swapchain> swapchain{};
    std::unique_ptr<ImguiWrapper> imgui{};
    std::unique_ptr<RenderTargets> renderTargets{};
    std::unique_ptr<ModelLoader> modelLoader{};

    uint64_t frameNumber{0};
    std::vector<FrameData> frameSynchronization;
    int32_t renderFramesInFlight{0};

    // Probably want separate descriptor buffers/layouts for:
    //  - Scene Data
    DescriptorSetLayout renderTargetSetLayout{};
    DescriptorBufferStorageImage renderTargetDescriptors{};
    //
    // VkDescriptorSetLayout bindlessUniformSetLayout{};
    // std::unique_ptr<DescriptorBufferUniform> bindlessUniforms{};
    // VkDescriptorSetLayout bindlessCombinedImageSamplerSetLayout{};
    // std::unique_ptr<DescriptorBufferCombinedImageSampler> bindlessCombinedImageSamplers{};
    // VkDescriptorSetLayout bindlessStorageImageSetLayout{};
    // std::unique_ptr<DescriptorBufferStorageImage> bindlessStorageImages{};


    std::vector<ModelData> modelDatas{};

    AllocatedBuffer megaVertexBuffer;
    OffsetAllocator::Allocator vertexBufferAllocator{sizeof(Vertex) * MEGA_VERTEX_BUFFER_COUNT};
    AllocatedBuffer megaIndexBuffer;
    OffsetAllocator::Allocator indexBufferAllocator{sizeof(uint32_t) * MEGA_INDEX_BUFFER_COUNT};
    AllocatedBuffer materialBuffer;
    OffsetAllocator::Allocator materialBufferAllocator{sizeof(MaterialProperties) * MEGA_MATERIAL_BUFFER_COUNT};
    AllocatedBuffer primitiveBuffer;
    OffsetAllocator::Allocator primitiveBufferAllocator{sizeof(MaterialProperties) * MEGA_PRIMITIVE_BUFFER_COUNT};

    AllocatedBuffer modelBuffer;
    AllocatedBuffer instanceBuffer;
    std::vector<Instance> instances;

    uint32_t highestInstanceIndex{0};
    std::vector<AllocatedBuffer> opaqueIndexedIndirectBuffers;
    std::vector<AllocatedBuffer> indirectCountBuffers; // size = FIF

    PipelineLayout drawCullPipelineLayout;
    Pipeline drawCullPipeline;
    PipelineLayout renderPipelineLayout;
    Pipeline renderPipeline;


    float cameraPos[3]{0.0f, 0.0f, -2.0f};
    float cameraLook[3]{0.0f, 0.0f, 0.0f};
    float boxPos[3]{0.0f, 0.0f, 0.0f};

    bool bShouldExit{false};

    // Render Information
    bool bSwapchainOutdated{false};
    std::unique_ptr<RenderContext> renderContext{};

};
}


#endif //WILLENGINETESTBED_MODEL_LOADING_H
