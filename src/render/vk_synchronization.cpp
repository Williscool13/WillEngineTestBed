//
// Created by William on 2025-10-10.
//

#include "vk_synchronization.h"

#include "render_utils.h"
#include "vk_context.h"
#include "vk_helpers.h"

namespace Renderer
{
FrameData::FrameData(VulkanContext* context)
    : context(context)
{}

FrameData::~FrameData()
{
    if (context && commandPool != VK_NULL_HANDLE) {
        // Command buffer is freed when pool is destroyed.
        vkDestroyCommandPool(context->device, commandPool, nullptr);
        vkDestroyFence(context->device, renderFence, nullptr);
        vkDestroySemaphore(context->device, swapchainSemaphore, nullptr);
        vkDestroySemaphore(context->device, renderSemaphore, nullptr);
    }
}

FrameData::FrameData(FrameData&& other) noexcept
{
    context = other.context;
    commandPool = other.commandPool;
    commandBuffer = other.commandBuffer;
    renderFence = other.renderFence;
    swapchainSemaphore = other.swapchainSemaphore;
    renderSemaphore = other.renderSemaphore;

    other.context = nullptr;
    other.commandPool = VK_NULL_HANDLE;
    other.commandBuffer = VK_NULL_HANDLE;
    other.renderFence = VK_NULL_HANDLE;
    other.swapchainSemaphore = VK_NULL_HANDLE;
    other.renderSemaphore = VK_NULL_HANDLE;
}

FrameData& FrameData::operator=(FrameData&& other) noexcept
{
    if (this != &other) {
        if (context && commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context->device, commandPool, nullptr);
            vkDestroyFence(context->device, renderFence, nullptr);
            vkDestroySemaphore(context->device, swapchainSemaphore, nullptr);
            vkDestroySemaphore(context->device, renderSemaphore, nullptr);
        }

        context = other.context;
        commandPool = other.commandPool;
        commandBuffer = other.commandBuffer;
        renderFence = other.renderFence;
        swapchainSemaphore = other.swapchainSemaphore;
        renderSemaphore = other.renderSemaphore;

        other.context = nullptr;
        other.commandPool = VK_NULL_HANDLE;
        other.commandBuffer = VK_NULL_HANDLE;
        other.renderFence = VK_NULL_HANDLE;
        other.swapchainSemaphore = VK_NULL_HANDLE;
        other.renderSemaphore = VK_NULL_HANDLE;
    }
    return *this;
}

void FrameData::Initialize()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = VkHelpers::CommandPoolCreateInfo(context->graphicsQueueFamily);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = VkHelpers::CommandBufferAllocateInfo(1);
    VK_CHECK(vkCreateCommandPool(context->device, &commandPoolCreateInfo, nullptr, &commandPool));
    commandBufferAllocateInfo.commandPool = commandPool;
    VK_CHECK(vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo, &commandBuffer));

    const VkFenceCreateInfo fenceCreateInfo = VkHelpers::FenceCreateInfo();
    const VkSemaphoreCreateInfo semaphoreCreateInfo = VkHelpers::SemaphoreCreateInfo();
    VK_CHECK(vkCreateFence(context->device, &fenceCreateInfo, nullptr, &renderFence));
    VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &swapchainSemaphore));
    VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &renderSemaphore));
}
} // Renderer
