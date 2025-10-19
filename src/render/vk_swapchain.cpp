//
// Created by William on 2025-10-10.
//

#include "vk_swapchain.h"

#include "VkBootstrap.h"

#include "vk_context.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "render_constants.h"
#include "render_utils.h"

namespace Renderer
{
Swapchain::Swapchain(const VulkanContext* context, uint32_t width, uint32_t height): context(context)
{
    Create(width, height);
    Dump();
}

Swapchain::~Swapchain()
{
    vkDestroySwapchainKHR(context->device, handle, nullptr);
    for (VkImageView swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImageView, nullptr);
    }
}

void Swapchain::Create(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{context->physicalDevice, context->device, context->surface};

    uint32_t formatCount;
    VkSurfaceFormatKHR surfaceFormats[32]{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, surfaceFormats));

    VkFormat targetSwapchainFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR targetColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (int32_t i = 0; i < formatCount; ++i) {
        if (surfaceFormats[i].format == SWAPCHAIN_HDR_IMAGE_FORMAT) {
            targetSwapchainFormat = SWAPCHAIN_HDR_IMAGE_FORMAT;
            targetColorSpace = surfaceFormats[i].colorSpace;
            break;
        }
        if (surfaceFormats[i].format == SWAPCHAIN_SDR_IMAGE_FORMAT) {
            targetSwapchainFormat = SWAPCHAIN_SDR_IMAGE_FORMAT;
            targetColorSpace = surfaceFormats[i].colorSpace;
        }
    }

    if (targetSwapchainFormat == VK_FORMAT_UNDEFINED) {
        LOG_ERROR("Failed to get valid swapchains for this surface. Crashing.");
        CrashHandler::TriggerManualDump("Failed to find valid swpachain format");
        exit(1);
    }

    auto swapchainResult = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = targetSwapchainFormat, .colorSpace = targetColorSpace})
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_desired_min_image_count(vkb::SwapchainBuilder::TRIPLE_BUFFERING)
            //.set_desired_min_image_count(vkb::SwapchainBuilder::DOUBLE_BUFFERING)
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
    colorSpace = vkbSwapchain.color_space;
    extent = {vkbSwapchain.extent.width, vkbSwapchain.extent.height};
    swapchainImages = imagesResult.value();
    swapchainImageViews = viewsResult.value();
}

void Swapchain::Recreate(uint32_t width, uint32_t height)
{
    vkDestroySwapchainKHR(context->device, handle, nullptr);
    for (const auto swapchainImage : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImage, nullptr);
    }

    Create(width, height);
    Dump();
}

void Swapchain::Dump()
{
    LOG_INFO("=== Swapchain Info ===");
    LOG_INFO("Image Count: {}", imageCount);
    LOG_INFO("Format: {}", static_cast<uint32_t>(format));
    LOG_INFO("Color Space: {}", static_cast<uint32_t>(colorSpace));
    LOG_INFO("Extent: {}x{}", extent.width, extent.height);
    LOG_INFO("Images: {}", swapchainImages.size());
    LOG_INFO("Image Views: {}", swapchainImageViews.size());
}
} // Renderer
