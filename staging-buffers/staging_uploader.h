//
// Created by William on 2025-11-05.
//

#ifndef WILLENGINETESTBED_STAGING_UPLOADER_H
#define WILLENGINETESTBED_STAGING_UPLOADER_H

#include <volk/volk.h>

#include "offsetAllocator.hpp"
#include "render/render_constants.h"
#include "render/vk_resources.h"
#include "render/model/model_data.h"

class StagingUploader
{
public:
    StagingUploader();
    ~StagingUploader();
    StagingUploader(Renderer::VulkanContext* context);

    void UploadStaticData(
        Renderer::AllocatedBuffer& vertexBuffer,
        std::vector<Renderer::Vertex>& vertices,
        OffsetAllocator::Allocation vertexRealAllocation,
        Renderer::AllocatedBuffer& indexBuffer, std::vector<uint32_t>& indices, OffsetAllocator::Allocation indexRealAllocation, Renderer::AllocatedBuffer& materialBuffer, std::vector<Renderer::
        MaterialProperties>& materials, OffsetAllocator::Allocation materialRealAllocation, Renderer::AllocatedBuffer& primitiveBuffer, std::vector<Renderer::Primitive>& primitives, OffsetAllocator::
        Allocation primitiveRealAllocation);

private:
    Renderer::VulkanContext* context{};
    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer{};
    VkFence fence{};

    Renderer::AllocatedBuffer stagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{Renderer::STAGING_BUFFER_SIZE};
};


#endif //WILLENGINETESTBED_STAGING_UPLOADER_H