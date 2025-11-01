//
// Created by William on 2025-11-01.
//

#ifndef WILLENGINETESTBED_GRADIENT_COMPUTE_PIPELINE_H
#define WILLENGINETESTBED_GRADIENT_COMPUTE_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
class GradientComputePipeline
{
public:
    GradientComputePipeline();

    ~GradientComputePipeline();

    explicit GradientComputePipeline(VulkanContext* context, VkDescriptorSetLayout renderTargetSetLayout);

    GradientComputePipeline(const GradientComputePipeline&) = delete;

    GradientComputePipeline& operator=(const GradientComputePipeline&) = delete;

    GradientComputePipeline(GradientComputePipeline&& other) noexcept;

    GradientComputePipeline& operator=(GradientComputePipeline&& other) noexcept;

public:
    PipelineLayout gradientPipelineLayout;
    Pipeline gradientPipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_GRADIENT_COMPUTE_PIPELINE_H