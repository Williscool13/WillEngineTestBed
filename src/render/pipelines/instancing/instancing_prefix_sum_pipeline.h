//
// Created by William on 2025-11-29.
//

#ifndef WILLENGINETESTBED_INSTANCING_PREFIX_SUM_PIPELINE_H
#define WILLENGINETESTBED_INSTANCING_PREFIX_SUM_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct PrefixSumPushConstant
{
    VkDeviceAddress primitiveCountBuffer;
    uint32_t highestPrimitiveIndex;
};

class InstancingPrefixSumPipeline
{
public:
    InstancingPrefixSumPipeline();

    ~InstancingPrefixSumPipeline();

    explicit InstancingPrefixSumPipeline(VulkanContext* context);

    InstancingPrefixSumPipeline(const InstancingPrefixSumPipeline&) = delete;

    InstancingPrefixSumPipeline& operator=(const InstancingPrefixSumPipeline&) = delete;

    InstancingPrefixSumPipeline(InstancingPrefixSumPipeline&& other) noexcept;

    InstancingPrefixSumPipeline& operator=(InstancingPrefixSumPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_INSTANCING_PREFIX_SUM_PIPELINE_H