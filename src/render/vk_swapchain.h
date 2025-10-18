//
// Created by William on 2025-10-10.
//

#ifndef WILLENGINETESTBED_SWAPCHAIN_H
#define WILLENGINETESTBED_SWAPCHAIN_H
#include <vector>
#include <volk/volk.h>

#include "VkBootstrap.h"

namespace Renderer
{
struct VulkanContext;

struct Swapchain
{
    Swapchain() = delete;

    explicit Swapchain(const VulkanContext* context);

    ~Swapchain();

    void Dump();


    VkSwapchainKHR handle{};
    VkFormat format{};
    VkColorSpaceKHR colorSpace{};
    VkExtent2D extent{};
    uint32_t imageCount;
    std::vector<VkImage> swapchainImages{};
    std::vector<VkImageView> swapchainImageViews{};

private:
    const VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_SWAPCHAIN_H
