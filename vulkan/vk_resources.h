//
// Created by William on 2025-10-11.
//

#ifndef WILLENGINETESTBED_VK_RESOURCES_H
#define WILLENGINETESTBED_VK_RESOURCES_H

#include <volk/volk.h>
#include <vma/include/vk_mem_alloc.h>

namespace Renderer
{
struct VulkanContext;

struct Buffer
{
    VkBuffer handle{};
    VkDeviceAddress address{};
    size_t size{};

    VmaAllocation allocation{};
    VmaAllocationInfo allocationInfo{};

    void Cleanup(const VulkanContext* context);
};
}

#endif //WILLENGINETESTBED_VK_RESOURCES_H
