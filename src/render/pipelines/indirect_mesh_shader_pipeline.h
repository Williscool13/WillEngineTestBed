//
// Created by William on 2025-11-21.
//

#ifndef WILLENGINETESTBED_INDIRECT_MESH_SHADER_PIPELINE_H
#define WILLENGINETESTBED_INDIRECT_MESH_SHADER_PIPELINE_H

#include <volk/volk.h>
#include <glm/glm.hpp>

#include "render/vk_resources.h"

namespace Renderer
{
struct VulkanContext;

struct IndirectMeshShaderPushConstants
{
    VkDeviceAddress sceneData;

    // Statics
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress meshletVerticesBuffer;
    VkDeviceAddress meshletTrianglesBuffer;
    VkDeviceAddress meshletBuffer;

    VkDeviceAddress meshIndirectParameterBuffer;

    // Dynamics
    VkDeviceAddress materialBuffer; // well, this ones not yet dynamic
    VkDeviceAddress modelBuffer;
};


class IndirectMeshShaderPipeline
{
public:
    IndirectMeshShaderPipeline();

    ~IndirectMeshShaderPipeline();

    IndirectMeshShaderPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet);

    IndirectMeshShaderPipeline(const IndirectMeshShaderPipeline&) = delete;

    IndirectMeshShaderPipeline& operator=(const IndirectMeshShaderPipeline&) = delete;

    IndirectMeshShaderPipeline(IndirectMeshShaderPipeline&& other) noexcept;

    IndirectMeshShaderPipeline& operator=(IndirectMeshShaderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context{nullptr};
};
} // Renderer

#endif //WILLENGINETESTBED_INDIRECT_MESH_SHADER_PIPELINE_H