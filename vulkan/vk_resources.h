//
// Created by William on 2025-10-11.
//

#ifndef WILLENGINETESTBED_VK_RESOURCES_H
#define WILLENGINETESTBED_VK_RESOURCES_H

#include <volk/volk.h>
#include <vma/include/vk_mem_alloc.h>

#include "vulkan_context.h"

namespace Renderer
{
struct VulkanContext;

struct AllocatedBuffer
{
    VkBuffer handle{};
    VkDeviceAddress address{};
    size_t size{};

    VmaAllocation allocation{};
    VmaAllocationInfo allocationInfo{};

    void Cleanup(const VulkanContext* context);
};

struct AllocatedImage
{

    VkImage handle{};
    VkFormat format{};
    VkExtent3D extent{};
    VkImageLayout layout{};
    uint32_t mipLevels{};
    VmaAllocation allocation{};

    void Cleanup(const VulkanContext* context);
};

struct AllocatedImageView
{
    VkImageView handle{};

    void Cleanup(const VulkanContext* context);
};


namespace VkResources
{
    AllocatedImage CreateAllocatedImage(const VulkanContext* context, const VkImageCreateInfo& imageCreateInfo);
    AllocatedImageView CreateAllocatedImageView(const VulkanContext* context, const VkImageViewCreateInfo& imageViewCreateInfo);
}

}

#endif //WILLENGINETESTBED_VK_RESOURCES_H
