//
// Created by William on 2025-11-29.
//

#ifndef WILLENGINETESTBED_INSTANCING_INDIRECT_MESH_PIPELINE_H
#define WILLENGINETESTBED_INSTANCING_INDIRECT_MESH_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct IndirectMainDrawPushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress meshletVerticesBuffer;
    VkDeviceAddress meshletTrianglesBuffer;
    VkDeviceAddress meshletBuffer;
    VkDeviceAddress indirectBuffer;
    VkDeviceAddress compactedInstanceBuffer;
    VkDeviceAddress materialBuffer;
    VkDeviceAddress modelBuffer;
};

class InstancingIndirectMeshPipeline
{
public:
    InstancingIndirectMeshPipeline();

    ~InstancingIndirectMeshPipeline();

    explicit InstancingIndirectMeshPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessSetLayout);

    InstancingIndirectMeshPipeline(const InstancingIndirectMeshPipeline&) = delete;

    InstancingIndirectMeshPipeline& operator=(const InstancingIndirectMeshPipeline&) = delete;

    InstancingIndirectMeshPipeline(InstancingIndirectMeshPipeline&& other) noexcept;

    InstancingIndirectMeshPipeline& operator=(InstancingIndirectMeshPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_INSTANCING_INDIRECT_MESH_PIPELINE_H