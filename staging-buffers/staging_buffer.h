//
// Created by William on 2025-11-05.
//

#ifndef WILLENGINETESTBED_STAGING_BUFFER_H
#define WILLENGINETESTBED_STAGING_BUFFER_H
#include <memory>

#include "offsetAllocator.hpp"
#include <volk/volk.h>

#include "core/constants.h"
#include "render/render_constants.h"
#include "render/render_context.h"
#include "render/render_targets.h"
#include "render/resource_manager.h"
#include "render/vk_imgui_wrapper.h"
#include "render/vk_synchronization.h"
#include "render/vk_types.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/pipelines/draw_cull_compute_pipeline.h"
#include "render/pipelines/main_render_pipeline.h"


class StagingUploader;

namespace Renderer
{
struct Swapchain;
}

struct RuntimeMesh
{
    Renderer::ModelData* modelData{nullptr};
    // sorted when generated
    std::vector<Renderer::RuntimeNode> nodes;

    std::vector<uint32_t> nodeRemap{};

    Transform transform;
    OffsetAllocator::Allocation jointMatrixAllocation{};
    uint32_t jointMatrixOffset{0};
};

class StagingBuffer
{
public:
    StagingBuffer();

    ~StagingBuffer();

    void Initialize();

    void CreateResources();

    void Run();

    void Render();

    void Cleanup();

    bool LoadModelIntoBuffers(const std::filesystem::path& modelPath, Renderer::ModelData& modelData);

private:
    RuntimeMesh GenerateModel(Renderer::ModelData* modelData, const Transform& topLevelTransform);
    void UpdateTransforms(RuntimeMesh& runtimeMesh);

    void InitialUploadRuntimeMesh(RuntimeMesh& runtimeMesh);

private:
    SDL_Window* window{};
    std::unique_ptr<Renderer::VulkanContext> vulkanContext{};
    std::unique_ptr<Renderer::Swapchain> swapchain{};
    std::unique_ptr<Renderer::ImguiWrapper> imgui{};
    std::unique_ptr<Renderer::RenderTargets> renderTargets{};
    std::unique_ptr<Renderer::ModelLoader> modelLoader{};
    std::unique_ptr<StagingUploader> stagingUploader{};

    bool bSwapchainOutdated{false};
    std::unique_ptr<Renderer::RenderContext> renderContext{};
    bool bShouldExit{false};

    uint64_t frameNumber{0};
    int32_t renderFramesInFlight{0};
    std::vector<Renderer::FrameSynchronization> frameSynchronization;

    Renderer::ModelDataHandle suzanneHandle{Renderer::ModelDataHandle::Invalid};
    Renderer::ModelData suzanneData{};
    RuntimeMesh suzanneRuntimeMesh{};


    // Static Resources
    Renderer::AllocatedBuffer megaVertexBuffer;
    OffsetAllocator::Allocator vertexBufferAllocator{sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaIndexBuffer;
    OffsetAllocator::Allocator indexBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer materialBuffer;
    OffsetAllocator::Allocator materialBufferAllocator{sizeof(Renderer::MaterialProperties) * Renderer::MEGA_MATERIAL_BUFFER_COUNT};
    Renderer::AllocatedBuffer primitiveBuffer;
    OffsetAllocator::Allocator primitiveBufferAllocator{sizeof(Renderer::MaterialProperties) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT};

    // Runtime resources
    Renderer::SceneData sceneData;
    std::vector<Renderer::AllocatedBuffer> sceneDataBuffers;
    std::vector<Renderer::AllocatedBuffer> modelBuffers;
    std::vector<Renderer::AllocatedBuffer> instanceBuffers;
    std::vector<Renderer::AllocatedBuffer> jointMatrixBuffers;
    HandleAllocator<Renderer::ModelMatrix, Renderer::BINDLESS_MODEL_MATRIX_COUNT> modelMatrixAllocator;
    HandleAllocator<Renderer::InstanceEntry, Renderer::BINDLESS_INSTANCE_COUNT> instanceEntryAllocator;
    OffsetAllocator::Allocator jointMatrixAllocator{sizeof(Renderer::Model) * Renderer::BINDLESS_MODEL_MATRIX_COUNT};

    uint32_t highestInstanceIndex{0};
    Renderer::AllocatedBuffer opaqueIndexedIndirectBuffer;
    std::vector<Renderer::AllocatedBuffer> indirectCountBuffers;
    Renderer::AllocatedBuffer opaqueSkeletalIndexedIndirectBuffer;
    std::vector<Renderer::AllocatedBuffer> skeletalIndirectCountBuffers;

private:
    // Pipelines
    Renderer::DescriptorSetLayout renderTargetSetLayout{};
    Renderer::DescriptorBufferStorageImage renderTargetDescriptors{};
    Renderer::DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};

    Renderer::DrawCullComputePipeline drawCullComputePipeline;
    Renderer::MainRenderPipeline mainRenderPipeline;
};


#endif //WILLENGINETESTBED_STAGING_BUFFER_H
