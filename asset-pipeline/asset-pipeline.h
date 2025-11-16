//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_MULTIBUFFERING_H
#define WILLENGINETESTBED_MULTIBUFFERING_H

#include <memory>
#include <SDL3/SDL.h>

#include "core/data-structures/handle_allocator.h"
#include "render/render_constants.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/model/model_data.h"
#include "utils/utils.h"


namespace Renderer
{
class ModelLoader;
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

namespace AssetPipeline
{
class AssetPipeline
{
public:
    AssetPipeline();

    ~AssetPipeline();

    void Initialize();

    void Run();

    void Render(Renderer::FrameSynchronization& frameSync);

    void Cleanup();

    void CreateBuffers();

    void CreateMeshletModel();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> vulkanContext{};
    std::unique_ptr<Renderer::Swapchain> swapchain{};
    std::vector<Renderer::FrameSynchronization> frameSynchronization;
    uint64_t frameNumber{0};
    uint32_t renderFramesInFlight{0};

    bool bShouldExit{false};
    bool bSwapchainOutdated{false};

private:
    Renderer::DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};
    std::unique_ptr<Renderer::ModelLoader> modelLoader{};

    Renderer::AllocatedBuffer megaVertexBuffer;
    OffsetAllocator::Allocator vertexBufferAllocator{sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaMeshletVerticesBuffer;
    OffsetAllocator::Allocator meshletVerticesBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaMeshletTrianglesBuffer;
    OffsetAllocator::Allocator meshletTrianglesBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaMeshletBuffer;
    OffsetAllocator::Allocator meshletBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer materialBuffer;
    OffsetAllocator::Allocator materialBufferAllocator{sizeof(Renderer::MaterialProperties) * Renderer::MEGA_MATERIAL_BUFFER_COUNT};
    Renderer::AllocatedBuffer primitiveBuffer;
    OffsetAllocator::Allocator primitiveBufferAllocator{sizeof(Renderer::MaterialProperties) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT};

    HandleAllocator<Renderer::ModelMatrix, Renderer::BINDLESS_MODEL_MATRIX_COUNT> modelMatrixAllocator;
    std::vector<Renderer::AllocatedBuffer> modelBuffers;
    HandleAllocator<Renderer::InstanceEntry, Renderer::BINDLESS_INSTANCE_COUNT> instanceEntryAllocator;
    std::vector<Renderer::AllocatedBuffer> instanceBuffers;
    // Joint matrices need to be contiguous because indices are coded in vertices. We could modify vertex properties but...
    OffsetAllocator::Allocator jointMatrixAllocator{sizeof(Renderer::Model) * Renderer::BINDLESS_MODEL_MATRIX_COUNT};
    std::vector<Renderer::AllocatedBuffer> jointMatrixBuffers;

    Renderer::SceneData sceneData{};
    std::vector<Renderer::AllocatedBuffer> sceneDataBuffers;

    Renderer::MeshletModelData meshletModelData{};
};
}


#endif //WILLENGINETESTBED_MULTIBUFFERING_H
