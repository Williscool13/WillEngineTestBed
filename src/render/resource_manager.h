//
// Created by William on 2025-11-01.
//

#ifndef WILLENGINETESTBED_RESOURCE_MANAGER_H
#define WILLENGINETESTBED_RESOURCE_MANAGER_H


#include "render_constants.h"
#include "descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "model/model_data.h"
#include "utils/handle_allocator.h"


namespace Renderer
{
struct VulkanContext;
class ModelLoader;

class ResourceManager
{
public:
    ResourceManager();

    ~ResourceManager();

    ResourceManager(VulkanContext* context);

public:
    OffsetAllocator::Allocation AllocateVertices(size_t count);

    OffsetAllocator::Allocation AllocateIndices(size_t count);

    OffsetAllocator::Allocation AllocateMaterials(size_t count);

    OffsetAllocator::Allocation AllocatePrimitives(size_t count);

public:
    DescriptorBufferBindlessResources& GetResourceDescriptorBuffer() { return bindlessResourcesDescriptorBuffer; }

    AllocatedBuffer& GetMegaVertexBuffer() { return megaVertexBuffer; }
    AllocatedBuffer& GetMegaIndexBuffer() { return megaIndexBuffer; }
    AllocatedBuffer& GetMaterialBuffer() { return materialBuffer; }
    AllocatedBuffer& GetPrimitiveBuffer() { return primitiveBuffer; }

private:
    VulkanContext* context;

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
    OffsetAllocator::Allocator jointMatrixAllocator{sizeof(Model) * BINDLESS_MODEL_MATRIX_COUNT};
    std::vector<AllocatedBuffer> jointMatrixBuffers;

    DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};
};
} // Renderer

#endif //WILLENGINETESTBED_RESOURCE_MANAGER_H
