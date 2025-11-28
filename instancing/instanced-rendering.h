//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_MULTIBUFFERING_H
#define WILLENGINETESTBED_MULTIBUFFERING_H

#include <memory>
#include <SDL3/SDL.h>

#include "core/data-structures/handle_allocator.h"
#include "game/camera/free_camera.h"
#include "render/render_constants.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/model/model_data.h"
#include "render/pipelines/basic_mesh_shader_pipeline.h"
#include "render/pipelines/indirect_mesh_shader_pipeline.h"
#include "render/pipelines/main_mesh_shader_pipeline.h"
#include "render/pipelines/mesh_draw_cull_compute_pipeline.h"
#include "render/pipelines/render_pipeline.h"


namespace Renderer
{
struct RenderContext;
class ModelLoader;
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

namespace InstancedRendering
{
struct PackedVisibility
{
    uint32_t visibilityChunk;
};

struct InstancePrimitiveOffset
{
    uint16_t primitiveOffset;
};

struct PrimitiveCount
{
    uint32_t count;
    uint32_t offset;
};

struct VisibilityPushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;

    VkDeviceAddress packedVisibilityBuffer; // sizeof(instance / 32), visibility is packed into 32 bit chunks
    VkDeviceAddress instanceOffsetBuffer; // sizeof(instance) * PrimitiveCount
    VkDeviceAddress primitiveCountBuffer; // sizeof(primitive) * InstancePrimitiveOffset (increase to uint32 if want to have more than 65536 instances per primitive)
};

struct PrefixSumPushConstant
{
    VkDeviceAddress primitiveCountBuffer;
    uint32_t highestPrimitiveIndex;
};

struct InstancedMeshIndirectDrawParameters
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t compactedInstanceStart;

    uint32_t meshletOffset;
    uint32_t meshletCount;
    uint32_t materialIndex;
    uint32_t padding;
};

struct IndirectWritePushConstant
{
    // Read-Only
    VkDeviceAddress sceneData;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;

    VkDeviceAddress packedVisibilityBuffer;
    VkDeviceAddress instanceOffsetBuffer;
    VkDeviceAddress primitiveCountBuffer;

    // Read-Write
    VkDeviceAddress compactedInstanceBuffer;
    VkDeviceAddress indirectBuffer;
};

struct IndirectMainDrawPushConstant {
    VkDeviceAddress sceneData;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress meshletVerticesBuffer;
    VkDeviceAddress meshletTrianglesBuffer;
    VkDeviceAddress meshletBuffer;
    VkDeviceAddress indirectBuffer;
    VkDeviceAddress compactedInstanceBuffer;
    VkDeviceAddress materialBuffer;
    VkDeviceAddress modelBuffer;
};


class InstancedRendering
{
public:
    InstancedRendering();

    ~InstancedRendering();

    void TestShaders();

    void Initialize();

    void Run();

    void Render(uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync);

    void Cleanup();

    void CreateBuffers();

    Renderer::MeshletModelData CreateMeshletModel(const std::filesystem::path& path);

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> vulkanContext{};
    std::unique_ptr<Renderer::Swapchain> swapchain{};

    uint64_t frameNumber{0};
    std::unique_ptr<Renderer::RenderContext> renderContext{};
    std::vector<Renderer::FrameSynchronization> frameSynchronization;
    std::unique_ptr<Renderer::RenderTargets> renderTargets{};

    Game::FreeCamera freeCamera{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}};
    Renderer::SceneData sceneData{};
    std::vector<Renderer::AllocatedBuffer> sceneDataBuffers;

    bool bShouldExit{false};
    bool bSwapchainOutdated{false};

private:
    Renderer::DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};
    std::unique_ptr<Renderer::ModelLoader> modelLoader{};

    Renderer::AllocatedBuffer megaVertexBuffer;
    OffsetAllocator::Allocator vertexBufferAllocator{sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaMeshletVerticesBuffer;
    OffsetAllocator::Allocator meshletVerticesBufferAllocator{Renderer::MEGA_MESHLET_VERTEX_BUFFER_SIZE};
    Renderer::AllocatedBuffer megaMeshletTrianglesBuffer;
    OffsetAllocator::Allocator meshletTrianglesBufferAllocator{Renderer::MEGA_MESHLET_TRIANGLE_BUFFER_SIZE};
    Renderer::AllocatedBuffer megaMeshletBuffer;
    OffsetAllocator::Allocator meshletBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer materialBuffer;
    OffsetAllocator::Allocator materialBufferAllocator{sizeof(Renderer::MaterialProperties) * Renderer::MEGA_MATERIAL_BUFFER_COUNT};
    Renderer::AllocatedBuffer primitiveBuffer;
    OffsetAllocator::Allocator primitiveBufferAllocator{sizeof(Renderer::MeshletPrimitive) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT};

    HandleAllocator<Renderer::ModelMatrix, Renderer::BINDLESS_MODEL_MATRIX_COUNT> modelMatrixAllocator;
    Renderer::AllocatedBuffer modelBuffer;
    HandleAllocator<Renderer::InstanceEntry, Renderer::BINDLESS_INSTANCE_COUNT> instanceEntryAllocator;
    Renderer::AllocatedBuffer instanceBuffer;

    Renderer::MeshletModelData bunnyModel{};
    Renderer::MeshletModelData dragonModel{};

    Renderer::PipelineLayout instancingVisibilityPipelineLayout;
    Renderer::Pipeline instancingVisibilityPipeline;
    Renderer::PipelineLayout instancingPrefixSumPipelineLayout;
    Renderer::Pipeline instancingPrefixSumPipeline;
    Renderer::PipelineLayout instancingCompactAndGenerateIndirectPipelineLayout;
    Renderer::Pipeline instancingCompactAndGenerateIndirectPipeline;
    Renderer::PipelineLayout instanceIndirectMeshPipelineLayout;
    Renderer::Pipeline instanceIndirectMeshPipeline;

    Renderer::AllocatedBuffer packedVisibilityBuffer;
    Renderer::AllocatedBuffer instanceOffsetBuffer;
    Renderer::AllocatedBuffer primitiveCountBuffer;
    Renderer::AllocatedBuffer compactedInstanceBuffer;
    Renderer::AllocatedBuffer indirectBuffer;
};
}


#endif //WILLENGINETESTBED_MULTIBUFFERING_H
