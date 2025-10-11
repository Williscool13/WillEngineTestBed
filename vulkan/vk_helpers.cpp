//
// Created by William on 2025-10-10.
//

#include "vk_helpers.h"

namespace Renderer
{
VkImageMemoryBarrier2 VkHelpers::ImageMemoryBarrier(
    VkImage image,
    const VkImageSubresourceRange& subresourceRange,
    const VkPipelineStageFlagBits2 srcStageMask,
    const VkAccessFlagBits2 srcAccessMask,
    const VkImageLayout oldLayout,
    const VkPipelineStageFlagBits2 dstStageMask,
    const VkAccessFlagBits2 dstAccessMask,
    const VkImageLayout newLayout
)
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = subresourceRange,
    };
}

VkImageSubresourceRange VkHelpers::SubresourceRange(const VkImageAspectFlags aspectMask, const uint32_t levelCount, const uint32_t layerCount)
{
    return {
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = levelCount,
        .baseArrayLayer = 0,
        .layerCount = layerCount,
    };
}

VkDependencyInfo VkHelpers::DependencyInfo(VkImageMemoryBarrier2* imageBarrier)
{
    return {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = imageBarrier ? 1u : 0u,
        .pImageMemoryBarriers = imageBarrier,
    };
}

VkCommandPoolCreateInfo VkHelpers::CommandPoolCreateInfo()
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
}

VkCommandBufferAllocateInfo VkHelpers::CommandBufferAllocateInfo(uint32_t bufferCount, VkCommandPool commandPool)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = bufferCount,
    };
}

VkFenceCreateInfo VkHelpers::FenceCreateInfo()
{
    return {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
}

VkSemaphoreCreateInfo VkHelpers::SemaphoreCreateInfo()
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
    };
}

VkCommandBufferBeginInfo VkHelpers::CommandBufferBeginInfo()
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
}

VkCommandBufferSubmitInfo VkHelpers::CommandBufferSubmitInfo(VkCommandBuffer cmd)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
        .deviceMask = 0,
    };
}

VkSubmitInfo2 VkHelpers::SubmitInfo(VkCommandBufferSubmitInfo* commandBufferSubmitInfo, const VkSemaphoreSubmitInfo* waitSemaphoreInfo, const VkSemaphoreSubmitInfo* signalSemaphoreInfo)
{
    return {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0u : 1u,
        .pWaitSemaphoreInfos = waitSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = commandBufferSubmitInfo,
        .signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0u : 1u,
        .pSignalSemaphoreInfos = signalSemaphoreInfo,
    };
}

VkSemaphoreSubmitInfo VkHelpers::SemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask)
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stageMask,
        .deviceIndex = 0,
    };
}

VkPresentInfoKHR VkHelpers::PresentInfo(VkSwapchainKHR* swapchain, VkSemaphore* waitSemaphore, uint32_t* imageIndices)
{
    return {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = swapchain,
        .pImageIndices = imageIndices,
    };
}

VkDeviceSize VkHelpers::GetAlignedSize(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

VkDeviceAddress VkHelpers::GetDeviceAddress(VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = buffer;
    const uint64_t address = vkGetBufferDeviceAddress(device, &bufferDeviceAddressInfo);
    return address;
}

VkImageCreateInfo VkHelpers::ImageCreateInfo(VkFormat format, VkExtent3D extent, VkFlags usageFlags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,

        // Single 2D image with no mip levels by default
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,

        // No MSAA
        .samples = VK_SAMPLE_COUNT_1_BIT,

        // Tiling Optimal has the best performance
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usageFlags,
    };
}

VkImageViewCreateInfo VkHelpers::ImageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageSubresourceRange subresource{
        .aspectMask = aspectFlags,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = subresource,
    };
}
}
