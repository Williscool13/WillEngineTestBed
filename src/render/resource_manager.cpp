//
// Created by William on 2025-11-01.
//

#include "resource_manager.h"

#include "render/model/model_loader.h"

namespace Renderer
{
ResourceManager::ResourceManager() = default;

ResourceManager::~ResourceManager() = default;

ResourceManager::ResourceManager(VulkanContext* context)
    : context(context)
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.size = sizeof(Vertex) * MEGA_VERTEX_BUFFER_COUNT;
    megaVertexBuffer = VkResources::CreateAllocatedBuffer(context, bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.size = sizeof(uint32_t) * MEGA_INDEX_BUFFER_COUNT;
    megaIndexBuffer = VkResources::CreateAllocatedBuffer(context, bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.size = sizeof(MaterialProperties) * MEGA_MATERIAL_BUFFER_COUNT;
    materialBuffer = VkResources::CreateAllocatedBuffer(context, bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.size = sizeof(Primitive) * MEGA_PRIMITIVE_BUFFER_COUNT;
    primitiveBuffer = VkResources::CreateAllocatedBuffer(context, bufferInfo, vmaAllocInfo);

    bindlessResourcesDescriptorBuffer = DescriptorBufferBindlessResources(context);
}
} // Renderer
