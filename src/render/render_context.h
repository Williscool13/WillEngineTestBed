//
// Created by William on 2025-10-19.
//

#ifndef WILLENGINETESTBED_RENDER_CONTEXT_H
#define WILLENGINETESTBED_RENDER_CONTEXT_H

#include <array>
#include <cmath>
#include <volk/volk.h>
#include <glm/glm.hpp>

#include "render_constants.h"

namespace Renderer
{
struct RenderContext
{
    RenderContext(uint32_t width, uint32_t height, float scale)
    {
        renderExtents[0] = width;
        renderExtents[1] = height;
        renderScale = scale;

        aspectRatio = renderExtents[0] / static_cast<float>(renderExtents[1]);
        texelSize = {1.0f / renderExtents[0], 1 / renderExtents[1]};

        scaledRenderExtents[0] = static_cast<uint32_t>(std::round(width * scale));
        scaledRenderExtents[1] = static_cast<uint32_t>(std::round(height * scale));
    }

    void RequestRenderExtentResize(const uint32_t width, const uint32_t height)
    {
        pendingRenderWidth = width;
        pendingRenderHeight = height;
        bHasPendingRenderExtentChanges = true;
    }

    /**
     * Should be called when the draw images/render targets are remade. Will need to wait for `VkWaitForIdle`
     */
    void ApplyRenderExtentResize()
    {
        if (!bHasPendingRenderExtentChanges) { return; }
        renderExtents[0] = pendingRenderWidth;
        renderExtents[1] = pendingRenderHeight;

        aspectRatio = renderExtents[0] / static_cast<float>(renderExtents[1]);
        texelSize = {1.0f / renderExtents[0], 1 / renderExtents[1]};

        scaledRenderExtents[0] = static_cast<uint32_t>(std::round(renderExtents[0] * renderScale));
        scaledRenderExtents[1] = static_cast<uint32_t>(std::round(renderExtents[1] * renderScale));
        bHasPendingRenderExtentChanges = false;
    }

    void UpdateRenderScale(float newScale)
    {
        renderScale = newScale;
        scaledRenderExtents[0] = static_cast<uint32_t>(std::round(renderExtents[0] * renderScale));
        scaledRenderExtents[1] = static_cast<uint32_t>(std::round(renderExtents[1] * renderScale));
    }

    [[nodiscard]] bool HasPendingRenderExtentChanges() const { return bHasPendingRenderExtentChanges; }


    std::array<uint32_t, 2> GetRenderExtent() const { return renderExtents; }
    std::array<uint32_t, 2> GetScaledRenderExtent() const { return scaledRenderExtents; }
    float GetAspectRatio() const { return aspectRatio; }
    glm::vec2 GetTexelSize() const { return texelSize; }

private:
    float renderScale{1.0f};
    float aspectRatio{1920.0f / 1080};
    glm::vec2 texelSize{1 / 1920.0f, 1 / 1080.0f};

    // Calculated from the 3 above
    std::array<uint32_t, 2> renderExtents;
    std::array<uint32_t, 2> scaledRenderExtents;

    bool bHasPendingRenderExtentChanges{false};
    uint32_t pendingRenderWidth{0};
    uint32_t pendingRenderHeight{0};
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_CONTEXT_H
