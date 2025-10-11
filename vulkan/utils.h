//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_UTILS_H
#define WILLENGINETESTBED_UTILS_H

#include <logger.h>
#include <vulkan/vk_enum_string_helper.h>
#include <volk.h>

namespace Renderer
{

} // Renderer
inline static int32_t SWAPCHAIN_MINIMUM_IMAGE_COUNT = 2;
inline static int32_t SWAPCHAIN_DESIRED_IMAGE_COUNT = 3;
inline static VkFormat SWAPCHAIN_IMAGE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err) {                                                           \
            LOG_ERROR("Detected Vulkan error: {}\n", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    } while (0)

#endif //WILLENGINETESTBED_UTILS_H