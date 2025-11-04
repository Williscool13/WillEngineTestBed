//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_LOADING_H
#define WILLENGINETESTBED_MODEL_LOADING_H
#include <memory>
#include <vector>

#include "model_loading_types.h"
#include "offsetAllocator.hpp"
#include "SDL3/SDL.h"

#include "render/render_context.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "render/animation/animation_player.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/model/model_data.h"
#include "render/pipelines/draw_cull_compute_pipeline.h"
#include "utils/handle_allocator.h"

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

    void CreateModels();

    void Initialize();

    void Run();

    void Render();

    void Cleanup();

private:
    bool LoadModelIntoBuffers(const std::filesystem::path& modelPath, ModelData& modelData);

    RuntimeMesh GenerateModel(ModelDataHandle modelDataHandle, const Transform& topLevelTransform);

    void UpdateTransforms(RuntimeMesh& runtimeMesh);

    /**
     * Uploads runtime mesh properties to model and instance buffers. \n
     * Uploads to all at once. Will cause synchronization problems if run during render loop (without waiting for device idle).
     * @param runtimeMesh
     */
    void InitialUploadRuntimeMesh(RuntimeMesh& runtimeMesh);

    void UpdateRuntimeMesh(RuntimeMesh& runtimeMesh, const AllocatedBuffer& modelBuffer, const AllocatedBuffer& jointMatrixBuffer);

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<VulkanContext> vulkanContext{};
    std::unique_ptr<Swapchain> swapchain{};
    std::unique_ptr<ImguiWrapper> imgui{};
    std::unique_ptr<RenderTargets> renderTargets{};
    std::unique_ptr<ModelLoader> modelLoader{};

    uint64_t frameNumber{0};
    std::vector<FrameSynchronization> frameSynchronization;
    int32_t renderFramesInFlight{0};

    DescriptorSetLayout renderTargetSetLayout{};
    DescriptorBufferStorageImage renderTargetDescriptors{};

    DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};


    FreeList<ModelData, MAX_LOADED_MODELS> modelDatas{};
    std::vector<ModelDataHandle> modelDataHandles;
    std::vector<RuntimeMesh> runtimeMeshes{};
    ModelDataHandle simpleSkinHandle{};
    ModelDataHandle riggedFigureHandle{};

    RuntimeMesh* structureRuntimeMesh{};
    RuntimeMesh* simpleRiggedRuntimeMesh{};
    RuntimeMesh* riggedFigureRuntimeMesh{};

    AnimationPlayer animationPlayer{};
    AnimationPlayer animationPlayer2{};

    AllocatedBuffer megaVertexBuffer;
    OffsetAllocator::Allocator vertexBufferAllocator{sizeof(Vertex) * MEGA_VERTEX_BUFFER_COUNT};
    AllocatedBuffer megaIndexBuffer;
    OffsetAllocator::Allocator indexBufferAllocator{sizeof(uint32_t) * MEGA_INDEX_BUFFER_COUNT};
    // todo: multi-buffer material buffer for runtime modification
    AllocatedBuffer materialBuffer;
    OffsetAllocator::Allocator materialBufferAllocator{sizeof(MaterialProperties) * MEGA_MATERIAL_BUFFER_COUNT};
    AllocatedBuffer primitiveBuffer;
    OffsetAllocator::Allocator primitiveBufferAllocator{sizeof(MaterialProperties) * MEGA_PRIMITIVE_BUFFER_COUNT};

    HandleAllocator<ModelMatrix, BINDLESS_MODEL_MATRIX_COUNT> modelMatrixAllocator;
    std::vector<AllocatedBuffer> modelBuffers;
    HandleAllocator<InstanceEntry, BINDLESS_INSTANCE_COUNT> instanceEntryAllocator;
    std::vector<AllocatedBuffer> instanceBuffers;
    // Joint matrices need to be contiguous because indices are coded in vertices. We could modify vertex properties but...
    OffsetAllocator::Allocator jointMatrixAllocator{sizeof(Model) * BINDLESS_MODEL_MATRIX_COUNT};
    std::vector<AllocatedBuffer> jointMatrixBuffers;

    SceneData sceneData{};
    std::vector<AllocatedBuffer> sceneDataBuffers;

    uint32_t highestInstanceIndex{0};
    AllocatedBuffer opaqueIndexedIndirectBuffer;
    std::vector<AllocatedBuffer> indirectCountBuffers;
    AllocatedBuffer opaqueSkeletalIndexedIndirectBuffer;
    std::vector<AllocatedBuffer> skeletalIndirectCountBuffers;

    DrawCullComputePipeline drawCullComputePipeline;
    PipelineLayout renderPipelineLayout;
    Pipeline renderPipeline;
    PipelineLayout skeletalPipelineLayout;
    Pipeline skeletalPipeline;



    float cameraPos[3]{0.0f, 0.0f, -2.0f};
    float cameraLook[3]{0.0f, 0.0f, 0.0f};
    float boxPos[3]{0.0f, 0.0f, 0.0f};
    ModelDataHandle structureHandle;

    bool bShouldExit{false};

    // Render Information
    bool bSwapchainOutdated{false};
    std::unique_ptr<RenderContext> renderContext{};
};
}


#endif //WILLENGINETESTBED_MODEL_LOADING_H
