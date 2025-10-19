//
// Created by William on 2025-10-19.
//

#ifndef WILLENGINETESTBED_RENDER_TARGETS_H
#define WILLENGINETESTBED_RENDER_TARGETS_H
#include "vk_resources.h"

namespace Renderer
{
struct RenderTargets
{

    RenderTargets(VulkanContext* context, uint32_t width, uint32_t height);

    ~RenderTargets();

    void Create(uint32_t width, uint32_t height);

    void Recreate(uint32_t width, uint32_t height);

    AllocatedImage drawImage{};
    AllocatedImageView drawImageView{};
    AllocatedImage depthImage{};
    AllocatedImageView depthImageView{};

private:
    VulkanContext* context{};
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_TARGETS_H