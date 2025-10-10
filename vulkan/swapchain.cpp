//
// Created by William on 2025-10-10.
//

#include "swapchain.h"

#include "crash_handler.h"
#include "logger.h"
#include "utils.h"
#include "VkBootstrap.h"
#include "vulkan_context.h"

namespace Renderer
{
Swapchain::Swapchain(const VulkanContext* context)
    : context(context)
{
    vkb::SwapchainBuilder swapchainBuilder{context->physicalDevice, context->device, context->surface};

    auto swapchainResult = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = SWAPCHAIN_IMAGE_FORMAT, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            //.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(800, 600)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_required_min_image_count(vkb::SwapchainBuilder::DOUBLE_BUFFERING)
            .set_desired_min_image_count(vkb::SwapchainBuilder::TRIPLE_BUFFERING)
            .build();

    if (!swapchainResult) {
        LOG_CRITICAL("Failed to create swapchain: {}", swapchainResult.error().message());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    vkb::Swapchain vkbSwapchain = swapchainResult.value();

    auto imagesResult = vkbSwapchain.get_images();
    auto viewsResult = vkbSwapchain.get_image_views();

    if (!imagesResult || !viewsResult) {
        LOG_CRITICAL("Failed to get swapchain images/views");
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    handle = vkbSwapchain.swapchain;
    imageCount = vkbSwapchain.image_count;
    format = vkbSwapchain.image_format;
    extent = {vkbSwapchain.extent.width, vkbSwapchain.extent.height};
    swapchainImages = imagesResult.value();
    swapchainImageViews = viewsResult.value();
}

Swapchain::~Swapchain()
{
    vkDestroySwapchainKHR(context->device, handle, nullptr);
    for (VkImageView swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImageView, nullptr);
    }
}

void Swapchain::Dump()
{
    LOG_INFO("=== Swapchain Info ===");
    LOG_INFO("Image Count: {}", imageCount);
    LOG_INFO("Format: {}", static_cast<uint32_t>(format));
    LOG_INFO("Extent: {}x{}", extent.width, extent.height);
    LOG_INFO("Images: {}", swapchainImages.size());
    LOG_INFO("Image Views: {}", swapchainImageViews.size());
}
} // Renderer
