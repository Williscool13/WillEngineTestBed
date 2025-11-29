//
// Created by William on 2025-11-29.
//

#ifndef WILLENGINETESTBED_INSTANCING_COMPACT_AND_GENERATE_INDIRECT_PIPELINE_H
#define WILLENGINETESTBED_INSTANCING_COMPACT_AND_GENERATE_INDIRECT_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct IndirectWritePushConstant
{
    // Read-Only
    VkDeviceAddress sceneData;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;

    VkDeviceAddress packedVisibilityBuffer;
    VkDeviceAddress instanceOffsetBuffer;
    VkDeviceAddress primitiveCountBuffer;

    // Read-Write
    VkDeviceAddress compactedInstanceBuffer;
    VkDeviceAddress indirectBuffer;
};

class InstancingCompactAndGenerateIndirectPipeline
{
public:
    InstancingCompactAndGenerateIndirectPipeline();

    ~InstancingCompactAndGenerateIndirectPipeline();

    explicit InstancingCompactAndGenerateIndirectPipeline(VulkanContext* context);

    InstancingCompactAndGenerateIndirectPipeline(const InstancingCompactAndGenerateIndirectPipeline&) = delete;

    InstancingCompactAndGenerateIndirectPipeline& operator=(const InstancingCompactAndGenerateIndirectPipeline&) = delete;

    InstancingCompactAndGenerateIndirectPipeline(InstancingCompactAndGenerateIndirectPipeline&& other) noexcept;

    InstancingCompactAndGenerateIndirectPipeline& operator=(InstancingCompactAndGenerateIndirectPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_INSTANCING_COMPACT_AND_GENERATE_INDIRECT_PIPELINE_H