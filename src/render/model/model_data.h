//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_H
#define WILLENGINETESTBED_MODEL_H

#include <volk/volk.h>

#include "render/vk_resources.h"
#include "render/vk_types.h"

namespace Renderer
{
struct ModelData
{
    std::vector<Sampler> samplers{};
    std::vector<AllocatedImage> images{};
    std::vector<ImageView> imageViews{};
    std::vector<MaterialProperties> materials{};

    // Split for passes that only require position (shadow pass, depth prepass)
    VkBuffer vertexPositionBuffer{};
    VkBuffer vertexPropertyBuffer{};
    VkBuffer indexBuffer{};
    VkBuffer primitiveBuffer{};
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_H