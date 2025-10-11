//
// Created by William on 2025-10-11.
//

#include "vk_resources.h"

#include "vulkan_context.h"

void Renderer::Buffer::Cleanup(const VulkanContext* context)
{
    if (handle != VK_NULL_HANDLE) {
        vmaDestroyBuffer(context->allocator, handle, allocation);
        handle = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        address = 0;
        size = 0;
    }
}
