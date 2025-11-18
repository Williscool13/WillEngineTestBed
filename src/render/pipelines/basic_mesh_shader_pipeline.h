//
// Created by William on 2025-11-18.
//

#ifndef WILLENGINETESTBED_BASIC_MESH_SHADER_PIPELINE_H
#define WILLENGINETESTBED_BASIC_MESH_SHADER_PIPELINE_H

#include <volk/volk.h>
#include <glm/glm.hpp>

#include "render/vk_resources.h"

namespace Renderer
{
struct VulkanContext;

struct BasicMeshShaderPushConstants
{
    glm::mat4 modelMatrix;
    VkDeviceAddress sceneData;
};


class BasicMeshShaderPipeline
{
public:
    BasicMeshShaderPipeline();

    ~BasicMeshShaderPipeline();

    explicit BasicMeshShaderPipeline(VulkanContext* context);

    BasicMeshShaderPipeline(const BasicMeshShaderPipeline&) = delete;

    BasicMeshShaderPipeline& operator=(const BasicMeshShaderPipeline&) = delete;

    BasicMeshShaderPipeline(BasicMeshShaderPipeline&& other) noexcept;

    BasicMeshShaderPipeline& operator=(BasicMeshShaderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context{nullptr};
};
} // Renderer

#endif //WILLENGINETESTBED_BASIC_MESH_SHADER_PIPELINE_H