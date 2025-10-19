//
// Created by William on 2025-10-19.
//

#ifndef WILLENGINETESTBED_RENDER_CONTEXT_H
#define WILLENGINETESTBED_RENDER_CONTEXT_H

#include <array>
#include <cmath>
#include <volk/volk.h>

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
        if (!bHasPendingRenderExtentChanges ) {

        }
        renderExtents[0] = pendingRenderWidth;
        renderExtents[1] = pendingRenderHeight;

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


    std::array<uint32_t, 2> GetRenderExtent(){ return renderExtents; }
    std::array<uint32_t, 2> GetScaledRenderExtent(){ return scaledRenderExtents; }
private:
    float renderScale{1.0f};

    // Calculated from the 3 above
    std::array<uint32_t, 2> renderExtents;
    std::array<uint32_t, 2> scaledRenderExtents;

    bool bHasPendingRenderExtentChanges{false};
    uint32_t pendingRenderWidth{0};
    uint32_t pendingRenderHeight{0};
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_CONTEXT_H
