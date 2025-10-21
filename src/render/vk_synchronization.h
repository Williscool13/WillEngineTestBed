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
    VulkanContext* context{};

    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer{};
    VkFence renderFence{};
    VkSemaphore swapchainSemaphore{};
    VkSemaphore renderSemaphore{};

    FrameData() = default;
    explicit FrameData(VulkanContext* context);
    ~FrameData();

    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;

    FrameData(FrameData&& other) noexcept;
    FrameData& operator=(FrameData&& other) noexcept;

    void Initialize();
};
} // Renderer

#endif //WILLENGINETESTBED_SYNCHRONIZATION_H
