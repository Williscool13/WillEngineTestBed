//
// Created by William on 2025-10-19.
//

#include "descriptor_buffer_bindless_resources.h"

#include "crash-handling/logger.h"
#include "render/render_constants.h"
#include "render/vk_descriptors.h"
#include "render/vk_helpers.h"

namespace Renderer
{
DescriptorBufferBindlessResources::DescriptorBufferBindlessResources() = default;

DescriptorBufferBindlessResources::DescriptorBufferBindlessResources(VulkanContext* context)
    : context(context)
{
    DescriptorLayoutBuilder layoutBuilder{2};
    layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, BINDLESS_SAMPLER_COUNT);
    layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, BINDLESS_SAMPLED_IMAGE_COUNT);

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
        static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
    );
    descriptorSetLayout = VkResources::CreateDescriptorSetLayout(context, layoutCreateInfo);

    // Get size per descriptor set (Aligned).
    vkGetDescriptorSetLayoutSizeEXT(context->device, descriptorSetLayout.handle, &descriptorSetSize);
    descriptorSetSize = VkHelpers::GetAlignedSize(descriptorSetSize, VulkanContext::deviceInfo.descriptorBufferProps.descriptorBufferOffsetAlignment);

    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = descriptorSetSize;
    bufferInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    buffer = VkResources::CreateAllocatedBuffer(context, bufferInfo, vmaAllocInfo);

    freeSamplerIndices.reserve(BINDLESS_SAMPLER_COUNT);
    for (int32_t i = BINDLESS_SAMPLER_COUNT - 1; i >= 0; --i) {
        freeSamplerIndices.push_back(i);
    }

    freeTextureIndices.reserve(BINDLESS_SAMPLED_IMAGE_COUNT);
    for (int32_t i = BINDLESS_SAMPLED_IMAGE_COUNT - 1; i >= 0; --i) {
        freeTextureIndices.push_back(i);
    }
}

DescriptorBufferBindlessResources::~DescriptorBufferBindlessResources() = default;

DescriptorBufferBindlessResources::DescriptorBufferBindlessResources(DescriptorBufferBindlessResources&& other) noexcept
{
    buffer = std::move(other.buffer);

    context = other.context;
    descriptorSetLayout = std::move(other.descriptorSetLayout);
    descriptorSetSize = other.descriptorSetSize;

    freeSamplerIndices = std::move(other.freeSamplerIndices);
    freeTextureIndices = std::move(other.freeTextureIndices);
}

DescriptorBufferBindlessResources& DescriptorBufferBindlessResources::operator=(DescriptorBufferBindlessResources&& other) noexcept
{
    if (this != &other) {
        buffer = std::move(other.buffer);

        context = other.context;
        descriptorSetLayout = std::move(other.descriptorSetLayout);
        descriptorSetSize = other.descriptorSetSize;

        freeSamplerIndices = std::move(other.freeSamplerIndices);
        freeTextureIndices = std::move(other.freeTextureIndices);
    }

    return *this;
}

int32_t DescriptorBufferBindlessResources::AllocateSampler(VkSampler sampler)
{
    if (freeSamplerIndices.empty()) {
        LOG_WARN("No more sampler indices available to use in the sampler binding");
        return -1;
    }

    const int32_t samplerIndex = freeSamplerIndices.back();
    freeSamplerIndices.pop_back();

    // location = bufferAddress + setOffset + descriptorOffset + (arrayElement × descriptorSize)
    size_t setOffset = 0;
    size_t bindingOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, descriptorSetLayout.handle, 0, &bindingOffset);
    char* basePtr = static_cast<char*>(buffer.allocationInfo.pMappedData) + setOffset + bindingOffset;

    VkDescriptorGetInfoEXT descriptorGetInfo{};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorGetInfo.data.pSampler = &sampler;

    const size_t samplerDescriptorSize = VulkanContext::deviceInfo.descriptorBufferProps.samplerDescriptorSize;
    char* bufferPtr = basePtr + samplerIndex * samplerDescriptorSize;
    vkGetDescriptorEXT(context->device, &descriptorGetInfo, samplerDescriptorSize, bufferPtr);

    return samplerIndex;
}

int32_t DescriptorBufferBindlessResources::AllocateTexture(const VkDescriptorImageInfo& imageInfo)
{
    if (freeTextureIndices.empty()) {
        LOG_WARN("No more sampled image indices available to use in the sampler binding");
        return -1;
    }

    const int32_t textureIndex = freeTextureIndices.back();
    freeTextureIndices.pop_back();

    // location = bufferAddress + setOffset + descriptorOffset + (arrayElement × descriptorSize)
    size_t setOffset = 0;
    size_t bindingOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, descriptorSetLayout.handle, 1, &bindingOffset);
    char* basePtr = static_cast<char*>(buffer.allocationInfo.pMappedData) + setOffset + bindingOffset;

    VkDescriptorGetInfoEXT descriptorGetInfo{};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorGetInfo.data.pSampledImage = &imageInfo;

    const size_t samplerDescriptorSize = VulkanContext::deviceInfo.descriptorBufferProps.sampledImageDescriptorSize;
    char* bufferPtr = basePtr + textureIndex * samplerDescriptorSize;
    vkGetDescriptorEXT(context->device, &descriptorGetInfo, samplerDescriptorSize, bufferPtr);

    return textureIndex;
}

void DescriptorBufferBindlessResources::ReleaseSamplerBinding(int32_t bindingArrayIndex)
{
    if (std::ranges::find(freeSamplerIndices, bindingArrayIndex) != freeSamplerIndices.end()) {
        LOG_ERROR("[DescriptorBufferUniform] Descriptor set {} is already unallocated", bindingArrayIndex);
        return;
    }

    freeSamplerIndices.push_back(bindingArrayIndex);
}

void DescriptorBufferBindlessResources::ReleaseTextureBinding(int32_t bindingArrayIndex)
{
    if (std::ranges::find(freeTextureIndices, bindingArrayIndex) != freeTextureIndices.end()) {
        LOG_ERROR("[DescriptorBufferUniform] Descriptor set {} is already unallocated", bindingArrayIndex);
        return;
    }

    freeTextureIndices.push_back(bindingArrayIndex);
}

VkDescriptorBufferBindingInfoEXT DescriptorBufferBindlessResources::GetBindingInfo() const
{
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo{};
    descriptorBufferBindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    descriptorBufferBindingInfo.address = buffer.address;
    descriptorBufferBindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    return descriptorBufferBindingInfo;
}
} // Renderer
