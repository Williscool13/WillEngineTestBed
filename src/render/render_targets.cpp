//
// Created by William on 2025-10-19.
//

#include "render_targets.h"

#include "render_constants.h"
#include "vk_helpers.h"

namespace Renderer
{
RenderTargets::RenderTargets(VulkanContext* context, uint32_t width, uint32_t height)
    : context(context)
{
    Create(width, height);
}

RenderTargets::~RenderTargets()
{
}

void RenderTargets::Create(uint32_t width, uint32_t height)
{
    //
    {
        VkImageUsageFlags drawImageUsages{};
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo drawImageCreateInfo = VkHelpers::ImageCreateInfo(DRAW_IMAGE_FORMAT, {width, height, 1}, drawImageUsages);
        drawImage = VkResources::CreateAllocatedImage(context, drawImageCreateInfo);

        VkImageViewCreateInfo drawImageViewCreateInfo = VkHelpers::ImageViewCreateInfo(drawImage.handle, DRAW_IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
        drawImageView = VkResources::CreateImageView(context, drawImageViewCreateInfo);
    }

    //
    {
        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo depthImageCreateInfo = VkHelpers::ImageCreateInfo(DEPTH_IMAGE_FORMAT, {width, height, 1}, depthImageUsages);
        depthImage = VkResources::CreateAllocatedImage(context, depthImageCreateInfo);

        VkImageViewCreateInfo depthImageViewCreateInfo = VkHelpers::ImageViewCreateInfo(depthImage.handle, DEPTH_IMAGE_FORMAT, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView = VkResources::CreateImageView(context, depthImageViewCreateInfo);
    }
}
void RenderTargets::Recreate(uint32_t width, uint32_t height)
{
    Create(width, height);
}
} // Renderer
