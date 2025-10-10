//
// Created by William on 2025-10-10.
//

#ifndef WILLENGINETESTBED_VK_HELPERS_H
#define WILLENGINETESTBED_VK_HELPERS_H

#include <volk/volk.h>

namespace Renderer::VkHelpers
{
VkImageMemoryBarrier2 ImageMemoryBarrier(VkImage image, const VkImageSubresourceRange& subresourceRange, VkPipelineStageFlagBits2 srcStageMask, VkAccessFlagBits2 srcAccessMask, VkImageLayout
                                         oldLayout, VkPipelineStageFlagBits2 dstStageMask, VkAccessFlagBits2
                                         dstAccessMask, VkImageLayout newLayout);

VkImageSubresourceRange SubresourceRange(VkImageAspectFlags aspectMask, uint32_t levelCount = VK_REMAINING_MIP_LEVELS, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS);

VkDependencyInfo DependencyInfo(VkImageMemoryBarrier2* imageBarrier);

VkCommandPoolCreateInfo CommandPoolCreateInfo();

VkCommandBufferAllocateInfo CommandBufferAllocateInfo(uint32_t bufferCount, VkCommandPool commandPool = VK_NULL_HANDLE);

VkFenceCreateInfo FenceCreateInfo();

VkSemaphoreCreateInfo SemaphoreCreateInfo();

VkCommandBufferBeginInfo CommandBufferBeginInfo();

VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);

VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo* commandBufferSubmitInfo, const VkSemaphoreSubmitInfo* waitSemaphoreInfo, const VkSemaphoreSubmitInfo* signalSemaphoreInfo);

VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask);

VkPresentInfoKHR PresentInfo(VkSwapchainKHR* swapchain, VkSemaphore* waitSemaphore, uint32_t* imageIndices);
}


#endif //WILLENGINETESTBED_VK_HELPERS_H
