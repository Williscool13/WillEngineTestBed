//
// Created by William on 2025-10-16.
//

#ifndef WILLENGINETESTBED_RENDER_UTILS_H
#define WILLENGINETESTBED_RENDER_UTILS_H

#include <crash-handling/crash_handler.h>
#include <crash-handling/logger.h>
#include <vulkan/vk_enum_string_helper.h>
#include <volk.h>

namespace Renderer
{
#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err) {                                                           \
            LOG_ERROR("Detected Vulkan error: {}\n", string_VkResult(err));  \
            CrashHandler::TriggerManualDump("Vulkan Error");                 \
            exit(1);                                                         \
        }                                                                    \
} while (0)
} // Renderer

#endif //WILLENGINETESTBED_RENDER_UTILS_H