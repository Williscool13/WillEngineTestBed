//
// Created by William on 2025-10-10.
//

#include "synchronization.h"

#include "vulkan_context.h"

namespace Renderer
{
void FrameData::Cleanup(const VulkanContext* context) const
{
    vkDestroyCommandPool(context->device, commandPool, nullptr);

    vkDestroyFence(context->device, renderFence, nullptr);
    vkDestroySemaphore(context->device, renderSemaphore, nullptr);
    vkDestroySemaphore(context->device, swapchainSemaphore, nullptr);
}
} // Renderer
