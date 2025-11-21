//
// Created by William on 2025-11-21.
//

#ifndef WILLENGINETESTBED_MESH_DRAW_CULL_COMPUTE_PIPELINE_H
#define WILLENGINETESTBED_MESH_DRAW_CULL_COMPUTE_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct MeshDrawCullComputePushConstant
{
    VkDeviceAddress sceneData;

    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress instanceBuffer;

    VkDeviceAddress taskIndirectParameterBuffer;
};

class MeshDrawCullComputePipeline
{
public:
    MeshDrawCullComputePipeline();

    ~MeshDrawCullComputePipeline();

    explicit MeshDrawCullComputePipeline(VulkanContext* context);

    MeshDrawCullComputePipeline(const MeshDrawCullComputePipeline&) = delete;

    MeshDrawCullComputePipeline& operator=(const MeshDrawCullComputePipeline&) = delete;

    MeshDrawCullComputePipeline(MeshDrawCullComputePipeline&& other) noexcept;

    MeshDrawCullComputePipeline& operator=(MeshDrawCullComputePipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_MESH_DRAW_CULL_COMPUTE_PIPELINE_H