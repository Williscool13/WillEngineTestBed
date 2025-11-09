//
// Created by William on 2025-11-05.
//

#ifndef WILLENGINETESTBED_MAIN_PIPELINE_RENDER_H
#define WILLENGINETESTBED_MAIN_PIPELINE_RENDER_H
#include "render/vk_resources.h"

namespace Renderer
{
class MainRenderPipeline
{
public:
    MainRenderPipeline();

    ~MainRenderPipeline();

    explicit MainRenderPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet);

    MainRenderPipeline(const MainRenderPipeline&) = delete;

    MainRenderPipeline& operator=(const MainRenderPipeline&) = delete;

    MainRenderPipeline(MainRenderPipeline&& other) noexcept;

    MainRenderPipeline& operator=(MainRenderPipeline&& other) noexcept;


public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //WILLENGINETESTBED_MAIN_PIPELINE_RENDER_H