//
// Created by William on 2025-10-16.
//

#ifndef WILLENGINETESTBED_RENDER_CONSTANTS_H
#define WILLENGINETESTBED_RENDER_CONSTANTS_H

#include <volk/volk.h>
#include <cstdint>

namespace Renderer
{
inline static constexpr int32_t SWAPCHAIN_MINIMUM_IMAGE_COUNT = 2;
inline static constexpr int32_t SWAPCHAIN_DESIRED_IMAGE_COUNT = 3;
inline static constexpr bool ENABLE_HDR = false;
inline static constexpr VkFormat SWAPCHAIN_HDR_IMAGE_FORMAT = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
inline static constexpr VkColorSpaceKHR SWAPCHAIN_HDR_COLOR_SPACE = VK_COLOR_SPACE_HDR10_ST2084_EXT;
inline static constexpr VkFormat SWAPCHAIN_SDR_IMAGE_FORMAT = VK_FORMAT_B8G8R8A8_SRGB;
inline static constexpr VkColorSpaceKHR SWAPCHAIN_SDR_COLOR_SPACE = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
inline static constexpr VkFormat DRAW_IMAGE_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
inline static constexpr VkFormat DEPTH_IMAGE_FORMAT = VK_FORMAT_D32_SFLOAT;
inline static constexpr int32_t BINDLESS_UNIFORM_BUFFER_COUNT = 1000;
inline static constexpr int32_t BINDLESS_COMBINED_IMAGE_SAMPLER_COUNT = 1000;
inline static constexpr int32_t BINDLESS_STORAGE_IMAGE_COUNT = 1000;

inline static constexpr uint32_t DEFAULT_WINDOW_WIDTH = 1700;
inline static constexpr uint32_t DEFAULT_WINDOW_HEIGHT = 900;
inline static constexpr float DEFAULT_RENDER_SCALE = 0.5f;

}



#endif //WILLENGINETESTBED_RENDER_CONSTANTS_H