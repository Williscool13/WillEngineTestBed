//
// Created by William on 2025-12-02.
//

#ifndef WILLENGINETESTBED_TEXT_RENDERING_PIPELINE_H
#define WILLENGINETESTBED_TEXT_RENDERING_PIPELINE_H

#include <glm/glm.hpp>

#include "render/vk_resources.h"

namespace Renderer
{
struct TextRenderingPushConstant
{
    VkDeviceAddress sceneData;
};


class TextRenderingPipeline
{
public:
    TextRenderingPipeline();

    ~TextRenderingPipeline();

    explicit TextRenderingPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet);

    TextRenderingPipeline(const TextRenderingPipeline&) = delete;

    TextRenderingPipeline& operator=(const TextRenderingPipeline&) = delete;

    TextRenderingPipeline(TextRenderingPipeline&& other) noexcept;

    TextRenderingPipeline& operator=(TextRenderingPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_TEXT_RENDERING_PIPELINE_H