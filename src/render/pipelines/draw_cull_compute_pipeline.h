//
// Created by William on 2025-11-01.
//

#ifndef WILLENGINETESTBED_DRAW_CULL_COMPUTE_PIPELINE_H
#define WILLENGINETESTBED_DRAW_CULL_COMPUTE_PIPELINE_H
#include "render/vk_resources.h"

namespace Renderer
{
class DrawCullComputePipeline
{
public:
    DrawCullComputePipeline();

    ~DrawCullComputePipeline();

    explicit DrawCullComputePipeline(VulkanContext* context);

    DrawCullComputePipeline(const DrawCullComputePipeline&) = delete;

    DrawCullComputePipeline& operator=(const DrawCullComputePipeline&) = delete;

    DrawCullComputePipeline(DrawCullComputePipeline&& other) noexcept;

    DrawCullComputePipeline& operator=(DrawCullComputePipeline&& other) noexcept;

public:
    PipelineLayout drawCullPipelineLayout;
    Pipeline drawCullPipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_DRAW_CULL_COMPUTE_PIPELINE_H
