//
// Created by William on 2025-11-18.
//

#ifndef WILLENGINETESTBED_MAIN_MESH_SHADER_PIPELINE_H
#define WILLENGINETESTBED_MAIN_MESH_SHADER_PIPELINE_H

#include <volk/volk.h>
#include <glm/glm.hpp>

#include "render/vk_resources.h"

namespace Renderer
{
struct VulkanContext;

struct MainMeshShaderPushConstants
{
    VkDeviceAddress sceneData;

    // Statics
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress meshletVerticesBuffer;
    VkDeviceAddress meshletTrianglesBuffer;
    VkDeviceAddress meshletBuffer;

    // Dynamics
    VkDeviceAddress materialBuffer; // well, this ones not yet dynamic
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;
};


class MainMeshShaderPipeline
{
public:
    MainMeshShaderPipeline();

    ~MainMeshShaderPipeline();

    MainMeshShaderPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet);

    MainMeshShaderPipeline(const MainMeshShaderPipeline&) = delete;

    MainMeshShaderPipeline& operator=(const MainMeshShaderPipeline&) = delete;

    MainMeshShaderPipeline(MainMeshShaderPipeline&& other) noexcept;

    MainMeshShaderPipeline& operator=(MainMeshShaderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context{nullptr};
};
} // Renderer

#endif //WILLENGINETESTBED_MAIN_MESH_SHADER_PIPELINE_H