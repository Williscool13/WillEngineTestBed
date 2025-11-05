//
// Created by William on 2025-11-05.
//

#include "staging_uploader.h"

#include <array>

#include "render/render_utils.h"
#include "render/vk_helpers.h"

StagingUploader::StagingUploader() = default;

StagingUploader::~StagingUploader()
{
    vkDestroyCommandPool(context->device, commandPool, nullptr);
    vkDestroyFence(context->device, fence, nullptr);
}

StagingUploader::StagingUploader(Renderer::VulkanContext* context)
    : context(context)
{
    VkCommandPoolCreateInfo poolInfo = Renderer::VkHelpers::CommandPoolCreateInfo(context->transferQueueFamily);
    VK_CHECK(vkCreateCommandPool(context->device, &poolInfo, nullptr, &commandPool));
    VkCommandBufferAllocateInfo cmdInfo = Renderer::VkHelpers::CommandBufferAllocateInfo(1, commandPool);
    VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdInfo, &commandBuffer));

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0, // Unsignaled
    };
    VK_CHECK(vkCreateFence(context->device, &fenceInfo, nullptr, &fence));
    stagingBuffer = Renderer::VkResources::CreateAllocatedStagingBuffer(context, Renderer::STAGING_BUFFER_SIZE);
}

void StagingUploader::UploadStaticData(
    Renderer::AllocatedBuffer& vertexBuffer, std::vector<Renderer::Vertex>& vertices, OffsetAllocator::Allocation vertexRealAllocation,
    Renderer::AllocatedBuffer& indexBuffer, std::vector<uint32_t>& indices, OffsetAllocator::Allocation indexRealAllocation,
    Renderer::AllocatedBuffer& materialBuffer, std::vector<Renderer::MaterialProperties>& materials, OffsetAllocator::Allocation materialRealAllocation,
    Renderer::AllocatedBuffer& primitiveBuffer, std::vector<Renderer::Primitive>& primitives, OffsetAllocator::Allocation primitiveRealAllocation)
{
    VK_CHECK(vkResetFences(context->device, 1, &fence));
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    VkCommandBuffer cmd = commandBuffer;
    VkCommandBufferBeginInfo commandBufferBeginInfo = Renderer::VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    OffsetAllocator::Allocation vertexStagingAllocation = stagingAllocator.allocate(vertices.size() * sizeof(Renderer::Vertex));
    OffsetAllocator::Allocation indexStagingAllocation = stagingAllocator.allocate(indices.size() * sizeof(uint32_t));
    OffsetAllocator::Allocation materialStagingAllocation = stagingAllocator.allocate(materials.size() * sizeof(Renderer::MaterialProperties));
    OffsetAllocator::Allocation primitiveStagingAllocation = stagingAllocator.allocate(primitives.size() * sizeof(Renderer::Primitive));

    memcpy(static_cast<char*>(stagingBuffer.allocationInfo.pMappedData) + vertexStagingAllocation.offset, vertices.data(), vertices.size() * sizeof(Renderer::Vertex));
    VkBufferCopy2 vertexCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = vertexStagingAllocation.offset,
        .dstOffset = vertexRealAllocation.offset,
        .size = vertices.size() * sizeof(Renderer::Vertex),
    };


    memcpy(static_cast<char*>(stagingBuffer.allocationInfo.pMappedData) + indexStagingAllocation.offset, indices.data(), indices.size() * sizeof(uint32_t));
    VkBufferCopy2 indexCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = indexStagingAllocation.offset,
        .dstOffset = indexRealAllocation.offset,
        .size = indices.size() * sizeof(uint32_t),
    };

    memcpy(static_cast<char*>(stagingBuffer.allocationInfo.pMappedData) + materialStagingAllocation.offset, materials.data(), materials.size() * sizeof(Renderer::MaterialProperties));
    VkBufferCopy2 materialCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = materialStagingAllocation.offset,
        .dstOffset = materialRealAllocation.offset,
        .size = materials.size() * sizeof(Renderer::MaterialProperties),
    };


    memcpy(static_cast<char*>(stagingBuffer.allocationInfo.pMappedData) + primitiveStagingAllocation.offset, primitives.data(), primitives.size() * sizeof(Renderer::Primitive));
    VkBufferCopy2 primitiveCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = primitiveStagingAllocation.offset,
        .dstOffset = primitiveRealAllocation.offset,
        .size = primitives.size() * sizeof(Renderer::Primitive),
    };

    VkCopyBufferInfo2 copyVertexInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = stagingBuffer.handle,
        .dstBuffer = vertexBuffer.handle,
        .regionCount = 1,
        .pRegions = &vertexCopy
    };
    vkCmdCopyBuffer2(cmd, &copyVertexInfo);
    VkCopyBufferInfo2 copyIndexInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = stagingBuffer.handle,
        .dstBuffer = indexBuffer.handle,
        .regionCount = 1,
        .pRegions = &indexCopy
    };
    vkCmdCopyBuffer2(cmd, &copyIndexInfo);
    VkCopyBufferInfo2 copyMaterialInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = stagingBuffer.handle,
        .dstBuffer = materialBuffer.handle,
        .regionCount = 1,
        .pRegions = &materialCopy
    };
    vkCmdCopyBuffer2(cmd, &copyMaterialInfo);
    VkCopyBufferInfo2 copyPrimitiveInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = stagingBuffer.handle,
        .dstBuffer = primitiveBuffer.handle,
        .regionCount = 1,
        .pRegions = &primitiveCopy
    };
    vkCmdCopyBuffer2(cmd, &copyPrimitiveInfo);

    VkBufferMemoryBarrier2 barriers[4];
    barriers[0] = Renderer::VkHelpers::BufferMemoryBarrier(
        vertexBuffer.handle,
        vertexRealAllocation.offset, vertices.size() * sizeof(Renderer::Vertex),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[0].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[0].dstQueueFamilyIndex = context->graphicsQueueFamily;
    barriers[1] = Renderer::VkHelpers::BufferMemoryBarrier(
        indexBuffer.handle,
        indexRealAllocation.offset, indices.size() * sizeof(uint32_t),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[1].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[1].dstQueueFamilyIndex = context->graphicsQueueFamily;
    barriers[2] = Renderer::VkHelpers::BufferMemoryBarrier(
        materialBuffer.handle,
        materialRealAllocation.offset, materials.size() * sizeof(Renderer::MaterialProperties),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[2].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[2].dstQueueFamilyIndex = context->graphicsQueueFamily;
    barriers[3] = Renderer::VkHelpers::BufferMemoryBarrier(
        primitiveBuffer.handle,
        primitiveRealAllocation.offset, primitives.size() * sizeof(Renderer::Primitive),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[3].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[3].dstQueueFamilyIndex = context->graphicsQueueFamily;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;
    depInfo.dependencyFlags = 0;
    depInfo.bufferMemoryBarrierCount = 4;
    depInfo.pBufferMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo commandBufferSubmitInfo = Renderer::VkHelpers::CommandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submitInfo = Renderer::VkHelpers::SubmitInfo(&commandBufferSubmitInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(context->transferQueue, 1, &submitInfo, fence));
    VK_CHECK(vkWaitForFences(context->device, 1, &fence, true, 1000000000));


}

