//
// Created by William on 2025-11-09.
//

#ifndef WILLENGINETESTBED_MAIN_SKELETAL_RENDER_PIPELINE_H
#define WILLENGINETESTBED_MAIN_SKELETAL_RENDER_PIPELINE_H
#include "render/vk_resources.h"

namespace Renderer
{
class MainSkeletalRenderPipeline
{
public:
    MainSkeletalRenderPipeline();

    ~MainSkeletalRenderPipeline();

    explicit MainSkeletalRenderPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet);

    MainSkeletalRenderPipeline(const MainSkeletalRenderPipeline&) = delete;

    MainSkeletalRenderPipeline& operator=(const MainSkeletalRenderPipeline&) = delete;

    MainSkeletalRenderPipeline(MainSkeletalRenderPipeline&& other) noexcept;

    MainSkeletalRenderPipeline& operator=(MainSkeletalRenderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer


#endif //WILLENGINETESTBED_MAIN_SKELETAL_RENDER_PIPELINE_H
