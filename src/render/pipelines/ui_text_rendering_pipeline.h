//
// Created by William on 2025-12-02.
//

#ifndef WILLENGINETESTBED_TEXT_RENDERING_PIPELINE_H
#define WILLENGINETESTBED_TEXT_RENDERING_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct TextRenderingPushConstant
{
    VkDeviceAddress sceneData;
};


class UITextRenderingPipeline
{
public:
    UITextRenderingPipeline();

    ~UITextRenderingPipeline();

    explicit UITextRenderingPipeline(VulkanContext* context, VkDescriptorSetLayout fontAtlasDescriptorSet, VkDescriptorSetLayout bindlessDescriptorSet);

    UITextRenderingPipeline(const UITextRenderingPipeline&) = delete;

    UITextRenderingPipeline& operator=(const UITextRenderingPipeline&) = delete;

    UITextRenderingPipeline(UITextRenderingPipeline&& other) noexcept;

    UITextRenderingPipeline& operator=(UITextRenderingPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_TEXT_RENDERING_PIPELINE_H