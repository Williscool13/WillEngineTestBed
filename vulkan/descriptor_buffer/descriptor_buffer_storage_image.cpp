//
// Created by William on 2025-10-11.
//

#include "descriptor_buffer_storage_image.h"

#include "vulkan/utils.h"
#include "vulkan/vk_helpers.h"
#include "vulkan/vulkan_context.h"

namespace Renderer
{
DescriptorBufferStorageImage::DescriptorBufferStorageImage() = default;

DescriptorBufferStorageImage::DescriptorBufferStorageImage(VulkanContext* context, VkDescriptorSetLayout setLayout, int32_t maxSetCount)
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
    VK_CHECK(vmaCreateBuffer(context->allocator, &bufferInfo, &vmaAllocInfo, &buffer.handle, &buffer.allocation, &buffer.allocationInfo));
    buffer.size = bufferInfo.size;
    buffer.address = VkHelpers::GetDeviceAddress(context->device, buffer.handle);
}

DescriptorBufferStorageImage::~DescriptorBufferStorageImage()
{
    buffer.Cleanup(context);
}

void DescriptorBufferStorageImage::ReleaseDescriptorSet(int32_t descriptorSetIndex)
{
    if (std::ranges::find(freeIndices, descriptorSetIndex) != freeIndices.end()) {
        LOG_ERROR("[DescriptorBufferUniform] Descriptor set {} is already unallocated", descriptorSetIndex);
        return;
    }

    freeIndices.push_back(descriptorSetIndex);
}

void DescriptorBufferStorageImage::ReleaseAllDescriptorSets()
{
    freeIndices.clear();
    for (int32_t i = 0; i < maxDescriptorSets; ++i) {
        freeIndices.push_back(i);
    }
}

int32_t DescriptorBufferStorageImage::AllocateDescriptorSet()
{
    if (freeIndices.empty()) {
        LOG_WARN("No more descriptor sets available to use in this descriptor buffer storage");
        return -1;
    }

    const int32_t descriptorSetIndex = freeIndices.back();
    freeIndices.pop_back();
    return descriptorSetIndex;
}

bool DescriptorBufferStorageImage::UpdateDescriptorSet(std::span<VkDescriptorImageInfo> imageInfos, int32_t descriptorSetIndex, int32_t descriptorBindingIndex)
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

    VkDescriptorGetInfoEXT descriptorGetInfo{};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;


    const size_t storageImageSize = VulkanContext::deviceInfo.descriptorBufferProps.storageImageDescriptorSize;
    for (int32_t i = 0; i < imageInfos.size(); i++) {
        descriptorGetInfo.data.pStorageImage = &imageInfos[i];

        char* bufferPtr = basePtr + i * storageImageSize;
        vkGetDescriptorEXT(context->device, &descriptorGetInfo, storageImageSize, bufferPtr);
    }

    return true;
}

bool DescriptorBufferStorageImage::UpdateDescriptor(const VkDescriptorImageInfo& imageInfo, int32_t descriptorSetIndex, int32_t descriptorBindingIndex, int32_t bindingArrayIndex)
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

    VkDescriptorGetInfoEXT descriptorGetInfo{};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorGetInfo.data.pStorageImage = &imageInfo;

    const size_t storageImageSize = VulkanContext::deviceInfo.descriptorBufferProps.storageImageDescriptorSize;
    char* bufferPtr = basePtr + bindingArrayIndex * storageImageSize;
    vkGetDescriptorEXT(context->device, &descriptorGetInfo, storageImageSize, bufferPtr);

    return true;
}

VkDescriptorBufferBindingInfoEXT DescriptorBufferStorageImage::GetBindingInfo() const
{
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo{};
    descriptorBufferBindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    descriptorBufferBindingInfo.address = buffer.address;
    descriptorBufferBindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    return descriptorBufferBindingInfo;
}
} // Renderer