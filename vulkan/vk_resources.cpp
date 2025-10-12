//
// Created by William on 2025-10-11.
//

#include "vk_resources.h"

#include "utils.h"
#include "vulkan_context.h"

void Renderer::AllocatedBuffer::Cleanup(const VulkanContext* context)
{
    if (handle != VK_NULL_HANDLE) {
        vmaDestroyBuffer(context->allocator, handle, allocation);
        handle = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        address = 0;
        size = 0;
    }
}

void Renderer::AllocatedImage::Cleanup(const VulkanContext* context)
{
    if (handle != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(context->allocator, handle, allocation);
        handle = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
    extent = {};
    format = VK_FORMAT_UNDEFINED;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
    mipLevels = 0;
}

void Renderer::AllocatedImageView::Cleanup(const VulkanContext* context)
{
    if (handle != VK_NULL_HANDLE) {
        vkDestroyImageView(context->device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
}

Renderer::AllocatedImage Renderer::VkResources::CreateAllocatedImage(const VulkanContext* context, const VkImageCreateInfo& imageCreateInfo)
{
    AllocatedImage newImage;
    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    newImage.format = imageCreateInfo.format;
    newImage.extent = imageCreateInfo.extent;
    newImage.mipLevels = imageCreateInfo.mipLevels;
    VK_CHECK(vmaCreateImage(context->allocator, &imageCreateInfo, &allocInfo, &newImage.handle, &newImage.allocation, nullptr));
    return newImage;
}

Renderer::AllocatedImageView Renderer::VkResources::CreateAllocatedImageView(const VulkanContext* context, const VkImageViewCreateInfo& imageViewCreateInfo)
{
    AllocatedImageView newImageView;
    VK_CHECK(vkCreateImageView(context->device, &imageViewCreateInfo, nullptr, &newImageView.handle));
    return newImageView;
}
