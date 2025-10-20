//
// Created by William on 2025-10-11.
//

#include "descriptor_buffer_storage.h"

#include "../render_utils.h"
#include "../vk_helpers.h"
#include "../vk_context.h"

namespace Renderer
{
DescriptorBufferStorage::DescriptorBufferStorage() = default;

DescriptorBufferStorage::DescriptorBufferStorage(VulkanContext* context, VkDescriptorSetLayout setLayout, int32_t maxSetCount)
    : context(context), descriptorSetLayout(setLayout)
{
    // Get size per descriptor set (Aligned).
    vkGetDescriptorSetLayoutSizeEXT(context->device, setLayout, &descriptorSetSize);
    descriptorSetSize = VkHelpers::GetAlignedSize(descriptorSetSize, VulkanContext::deviceInfo.descriptorBufferProps.descriptorBufferOffsetAlignment);

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
    buffer = VkResources::CreateAllocatedBuffer(context, bufferInfo, vmaAllocInfo);
}

DescriptorBufferStorage::DescriptorBufferStorage(DescriptorBufferStorage&& other) noexcept
{
    buffer = std::move(other.buffer);
    freeIndices = std::move(other.freeIndices);

    context = other.context;
    descriptorSetLayout = other.descriptorSetLayout;
    maxDescriptorSets = other.maxDescriptorSets;
    descriptorSetSize = other.descriptorSetSize;
}

DescriptorBufferStorage& DescriptorBufferStorage::operator=(DescriptorBufferStorage&& other) noexcept
{
    if (this != &other) {
        buffer = std::move(other.buffer);
        freeIndices = std::move(other.freeIndices);

        context = other.context;
        descriptorSetLayout = other.descriptorSetLayout;
        maxDescriptorSets = other.maxDescriptorSets;
        descriptorSetSize = other.descriptorSetSize;
    }

    return *this;
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

bool DescriptorBufferStorage::UpdateDescriptorSet(std::span<AllocatedBuffer> storageBuffers, int32_t descriptorSetIndex, int32_t descriptorBindingIndex)
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

    // location = bufferAddress + setOffset + descriptorOffset + (arrayElement × descriptorSize)
    size_t setOffset = descriptorSetIndex * descriptorSetSize;
    size_t bindingOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, descriptorSetLayout, descriptorBindingIndex, &bindingOffset);
    char* basePtr = static_cast<char*>(buffer.allocationInfo.pMappedData) + setOffset + bindingOffset;

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

        char* bufferPtr = basePtr + i * storageBufferSize;
        vkGetDescriptorEXT(context->device, &descriptorGetInfo, storageBufferSize, bufferPtr);
    }

    return true;
}

bool DescriptorBufferStorage::UpdateDescriptor(const AllocatedBuffer& storageBuffer, int32_t descriptorSetIndex, int32_t descriptorBindingIndex, int32_t bindingArrayIndex)
{
    if (descriptorSetIndex < 0 || descriptorSetIndex >= maxDescriptorSets) {
        LOG_ERROR("Invalid descriptor set index: {}", descriptorSetIndex);
        return false;
    }

    if (std::ranges::find(freeIndices, descriptorSetIndex) != freeIndices.end()) {
        LOG_ERROR("Descriptor set {} is not allocated", descriptorSetIndex);
        return false;
    }

    // location = bufferAddress + setOffset + descriptorOffset + (arrayElement × descriptorSize)
    size_t setOffset = descriptorSetIndex * descriptorSetSize;
    size_t bindingOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, descriptorSetLayout, descriptorBindingIndex, &bindingOffset);
    char* basePtr = static_cast<char*>(buffer.allocationInfo.pMappedData) + setOffset + bindingOffset;

    VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
    descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;
    descriptorAddressInfo.address = storageBuffer.address;
    descriptorAddressInfo.range = storageBuffer.size;

    VkDescriptorGetInfoEXT descriptorGetInfo{};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;

    const size_t storageBufferSize = VulkanContext::deviceInfo.descriptorBufferProps.storageBufferDescriptorSize;
    char* bufferPtr = basePtr + bindingArrayIndex * storageBufferSize;
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
