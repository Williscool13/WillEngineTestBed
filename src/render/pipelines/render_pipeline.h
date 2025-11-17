//
// Created by William on 2025-11-17.
//

#ifndef WILLENGINETESTBED_RENDER_PIPELINE_H
#define WILLENGINETESTBED_RENDER_PIPELINE_H
#include <volk/volk.h>
#include <glm/glm.hpp>

#include "render/vk_resources.h"

namespace Renderer
{
struct VulkanContext;

struct RenderPushConstants
{
    glm::mat4 modelMatrix;
    VkDeviceAddress sceneData;
};


class RenderPipeline
{
public:
    RenderPipeline();

    ~RenderPipeline();

    explicit RenderPipeline(VulkanContext* context);

    RenderPipeline(const RenderPipeline&) = delete;

    RenderPipeline& operator=(const RenderPipeline&) = delete;

    RenderPipeline(RenderPipeline&& other) noexcept;

    RenderPipeline& operator=(RenderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_PIPELINE_H