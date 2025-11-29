//
// Created by William on 2025-11-29.
//

#ifndef WILLENGINETESTBED_INSTANCING_VISIBILITY_PIPELINE_H
#define WILLENGINETESTBED_INSTANCING_VISIBILITY_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct VisibilityPushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;

    VkDeviceAddress packedVisibilityBuffer; // sizeof(instance / 32), visibility is packed into 32 bit chunks
    VkDeviceAddress instanceOffsetBuffer;  // sizeof(instance) * PrimitiveCount
    VkDeviceAddress primitiveCountBuffer;  // sizeof(primitive) * InstancePrimitiveOffset (increase to uint32 if you want to have more than 65536 instances per primitive)
};

class InstancingVisibilityPipeline
{
public:
    InstancingVisibilityPipeline();

    ~InstancingVisibilityPipeline();

    explicit InstancingVisibilityPipeline(VulkanContext* context);

    InstancingVisibilityPipeline(const InstancingVisibilityPipeline&) = delete;

    InstancingVisibilityPipeline& operator=(const InstancingVisibilityPipeline&) = delete;

    InstancingVisibilityPipeline(InstancingVisibilityPipeline&& other) noexcept;

    InstancingVisibilityPipeline& operator=(InstancingVisibilityPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_INSTANCING_VISIBILITY_PIPELINE_H