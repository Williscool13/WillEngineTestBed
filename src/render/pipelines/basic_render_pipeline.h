//
// Created by William on 2025-11-16.
//

#ifndef WILLENGINETESTBED_BASIC_RENDER_PIPELINE_H
#define WILLENGINETESTBED_BASIC_RENDER_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
class BasicRenderPipeline
{
public:
    BasicRenderPipeline();

    ~BasicRenderPipeline();

    explicit BasicRenderPipeline(VulkanContext* context);

    BasicRenderPipeline(const BasicRenderPipeline&) = delete;

    BasicRenderPipeline& operator=(const BasicRenderPipeline&) = delete;

    BasicRenderPipeline(BasicRenderPipeline&& other) noexcept;

    BasicRenderPipeline& operator=(BasicRenderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_BASIC_RENDER_PIPELINE_H