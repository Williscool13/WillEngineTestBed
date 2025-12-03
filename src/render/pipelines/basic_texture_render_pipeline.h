//
// Created by William on 2025-12-02.
//

#ifndef WILLENGINETESTBED_BASIC_TEXTURE_RENDER_PIPELINE_H
#define WILLENGINETESTBED_BASIC_TEXTURE_RENDER_PIPELINE_H

#include <glm/glm.hpp>

#include "render/vk_resources.h"

namespace Renderer
{
struct Render3PushConstants
{
    glm::mat4 modelMatrix;
    VkDeviceAddress sceneData;
    uint32_t textureIndex;
    uint32_t samplerIndex;
};


class BasicTextureRenderPipeline
{
public:
    BasicTextureRenderPipeline();

    ~BasicTextureRenderPipeline();

    explicit BasicTextureRenderPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet);

    BasicTextureRenderPipeline(const BasicTextureRenderPipeline&) = delete;

    BasicTextureRenderPipeline& operator=(const BasicTextureRenderPipeline&) = delete;

    BasicTextureRenderPipeline(BasicTextureRenderPipeline&& other) noexcept;

    BasicTextureRenderPipeline& operator=(BasicTextureRenderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_BASIC_TEXTURE_RENDER_PIPELINE_H
