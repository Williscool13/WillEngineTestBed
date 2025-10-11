//
// Created by William on 2025-10-11.
//

#include "descriptor_buffer_storage.h"

#include "vulkan/utils.h"
#include "vulkan/vk_helpers.h"
#include "vulkan/vulkan_context.h"

namespace Renderer
{
DescriptorBufferStorage::DescriptorBufferStorage() = default;

DescriptorBufferStorage::DescriptorBufferStorage(VulkanContext* context, VkDescriptorSetLayout setLayout, int32_t maxSetCount)
    : context(context)
{
    // Get size per descriptor set (Aligned).
    vkGetDescriptorSetLayoutSizeEXT(context->device, setLayout, &descriptorSetSize);
    descriptorSetSize = VkHelpers::GetAlignedSize(descriptorSetSize, VulkanContext::deviceInfo.descriptorBufferProps.descriptorBufferOffsetAlignment);
    // Get descriptor buffer ptr after offset (potential metadata)
    vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, setLayout, 0u, &metadataOffset);

    // Set up indices in the descriptor buffer
    freeIndices.clear();
    freeIndices.reserve(maxSetCount);
    this->maxDescriptorSets = maxSetCount;
    for (int32_t i = maxSetCount - 1; i >= 0; --i) {
        freeIndices.push_back(i);
    }

    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = descriptorSetSize * maxSetCount;
    bufferInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VK_CHECK(vmaCreateBuffer(context->allocator, &bufferInfo, &vmaAllocInfo, &buffer.handle, &buffer.allocation, &buffer.allocationInfo));
    buffer.size = bufferInfo.size;
    buffer.address = VkHelpers::GetDeviceAddress(context->device, buffer.handle);
}

DescriptorBufferStorage::~DescriptorBufferStorage()
{
    buffer.Cleanup(context);
}

void DescriptorBufferStorage::ReleaseDescriptorSet(int32_t descriptorSetIndex)
{
    if (std::ranges::find(freeIndices, descriptorSetIndex) != freeIndices.end()) {
        LOG_ERROR("[DescriptorBufferUniform] Descriptor set {} is already unallocated", descriptorSetIndex);
        return;
    }

    freeIndices.push_back(descriptorSetIndex);
}

void DescriptorBufferStorage::ReleaseAllDescriptorSets()
{
    freeIndices.clear();
    for (int32_t i = 0; i < maxDescriptorSets; ++i) {
        freeIndices.push_back(i);
    }
}

int32_t DescriptorBufferStorage::AllocateDescriptorSet()
{
    if (freeIndices.empty()) {
        LOG_WARN("No more descriptor sets available to use in this descriptor buffer storage");
        return -1;
    }

    const int32_t descriptorSetIndex = freeIndices.back();
    freeIndices.pop_back();
    return descriptorSetIndex;
}

bool DescriptorBufferStorage::UpdateDescriptorSet(std::span<Buffer> storageBuffers, int32_t descriptorSetIndex)
{
    if (descriptorSetIndex < 0 || descriptorSetIndex >= maxDescriptorSets) {
        LOG_ERROR("Invalid descriptor set index: {}", descriptorSetIndex);
        return false;
    }

    // Check if index is actually allocated (not in free list)
    if (std::ranges::find(freeIndices, descriptorSetIndex) != freeIndices.end()) {
        LOG_ERROR("Descriptor set {} is not allocated", descriptorSetIndex);
        return false;
    }

    // Base offset
    uint64_t offset = metadataOffset + descriptorSetIndex * descriptorSetSize;

    VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
    descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;

    VkDescriptorGetInfoEXT descriptorGetInfo{};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;

    size_t storageBufferSize = VulkanContext::deviceInfo.descriptorBufferProps.storageBufferDescriptorSize;
    for (int32_t i = 0; i < storageBuffers.size(); i++) {
        descriptorAddressInfo.address = storageBuffers[i].address;
        descriptorAddressInfo.range = storageBuffers[i].size;

        char* bufferPtr = static_cast<char*>(buffer.allocationInfo.pMappedData) + offset + i * storageBufferSize;
        vkGetDescriptorEXT(context->device, &descriptorGetInfo, storageBufferSize, bufferPtr);
    }

    return true;
}

bool DescriptorBufferStorage::UpdateDescriptor(const Buffer& storageBuffer, int32_t descriptorSetIndex, int32_t bindingIndex)
{
    if (descriptorSetIndex < 0 || descriptorSetIndex >= maxDescriptorSets) {
        LOG_ERROR("Invalid descriptor set index: {}", descriptorSetIndex);
        return false;
    }

    if (std::ranges::find(freeIndices, descriptorSetIndex) != freeIndices.end()) {
        LOG_ERROR("Descriptor set {} is not allocated", descriptorSetIndex);
        return false;
    }

    const uint64_t offset = metadataOffset + descriptorSetIndex * descriptorSetSize;
    const size_t storageBufferSize = VulkanContext::deviceInfo.descriptorBufferProps.storageBufferDescriptorSize;

    VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
    descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;
    descriptorAddressInfo.address = storageBuffer.address;
    descriptorAddressInfo.range = storageBuffer.size;

    VkDescriptorGetInfoEXT descriptorGetInfo{};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;

    char* bufferPtr = static_cast<char*>(buffer.allocationInfo.pMappedData) + offset + bindingIndex * storageBufferSize;
    vkGetDescriptorEXT(context->device, &descriptorGetInfo, storageBufferSize, bufferPtr);

    return true;
}

VkDescriptorBufferBindingInfoEXT DescriptorBufferStorage::GetBindingInfo() const
{
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo{};
    descriptorBufferBindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    descriptorBufferBindingInfo.address = buffer.address;
    descriptorBufferBindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    return descriptorBufferBindingInfo;
}
}
