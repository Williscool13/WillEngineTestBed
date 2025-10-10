//
// Created by William on 2025-10-10.
//

#ifndef WILLENGINETESTBED_SYNCHRONIZATION_H
#define WILLENGINETESTBED_SYNCHRONIZATION_H

#include <volk/volk.h>

namespace Renderer
{
struct VulkanContext;

struct FrameData
{
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence renderFence;
    VkSemaphore swapchainSemaphore;
    VkSemaphore renderSemaphore;

    void Cleanup(const VulkanContext* context) const;
};

class Synchronization
{};
} // Renderer

#endif //WILLENGINETESTBED_SYNCHRONIZATION_H
