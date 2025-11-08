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

// Asset Loading
inline static constexpr uint32_t MAX_LOADED_MODELS = 1024;
inline static constexpr int32_t MEGA_VERTEX_BUFFER_COUNT = 1048576; // 1 << 20
inline static constexpr int32_t MEGA_INDEX_BUFFER_COUNT = 2097152; // 1 << 21
inline static constexpr int32_t MEGA_PRIMITIVE_BUFFER_COUNT = 65536;
inline static constexpr int32_t MEGA_MATERIAL_BUFFER_COUNT = 16384;

inline static constexpr int32_t BINDLESS_MODEL_MATRIX_COUNT = 16384;
inline static constexpr int32_t BINDLESS_INSTANCE_COUNT = 131072;
inline static constexpr int32_t BINDLESS_UNIFORM_BUFFER_COUNT = 1000;
inline static constexpr int32_t BINDLESS_COMBINED_IMAGE_SAMPLER_COUNT = 1000;
inline static constexpr int32_t BINDLESS_STORAGE_IMAGE_COUNT = 1000;
inline static constexpr int32_t BINDLESS_SAMPLER_COUNT = 64;
inline static constexpr int32_t BINDLESS_SAMPLED_IMAGE_COUNT = 8192;

inline static constexpr int32_t ASSET_LOAD_ASYNC_COUNT = 4;
inline static constexpr int32_t ASSET_LOAD_QUEUE_COUNT = 64;
inline static constexpr int32_t ASSET_LOAD_INDEPENDENT_BARRIER_COUNT = 64;
inline static constexpr int32_t STAGING_BUFFER_SIZE = 2 * 64 * 1024 * 1024; // 2 x 64 MB (1x uncompressed 4k rgba8, or 4x 4k BC7)

// Swapchain / Render Context
inline static constexpr uint32_t DEFAULT_SWAPCHAIN_WIDTH = 1700;
inline static constexpr uint32_t DEFAULT_SWAPCHAIN_HEIGHT = 900;
inline static constexpr uint32_t DEFAULT_RENDER_TARGET_WIDTH = 1700;
inline static constexpr uint32_t DEFAULT_RENDER_TARGET_HEIGHT = 900;
inline static constexpr float DEFAULT_RENDER_SCALE = 1.0f;
inline static constexpr bool RENDER_TARGET_SIZE_EQUALS_SWAPCHAIN_SIZE = true;

inline static constexpr uint32_t FRAME_BUFFER_OPERATION_COUNT_LIMIT = 8192;


}



#endif //WILLENGINETESTBED_RENDER_CONSTANTS_H