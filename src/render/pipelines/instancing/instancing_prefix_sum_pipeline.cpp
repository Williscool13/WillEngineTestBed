//
// Created by William on 2025-11-29.
//

#include "instancing_prefix_sum_pipeline.h"

#include <filesystem>

#include "crash-handling/logger_helpers.h"
#include "render/vk_helpers.h"

namespace Renderer
{
InstancingPrefixSumPipeline::InstancingPrefixSumPipeline() = default;

InstancingPrefixSumPipeline::~InstancingPrefixSumPipeline() = default;

InstancingPrefixSumPipeline::InstancingPrefixSumPipeline(VulkanContext* context)
    : context(context)
{
    VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};
    computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutCreateInfo.pNext = nullptr;
    computePipelineLayoutCreateInfo.pSetLayouts = nullptr;
    computePipelineLayoutCreateInfo.setLayoutCount = 0;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PrefixSumPushConstant);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
    computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    pipelineLayout = VkResources::CreatePipelineLayout(context, computePipelineLayoutCreateInfo);

    VkShaderModule computeShader;
    std::filesystem::path shaderPath = {"shaders/instancingPrefixSum_compute.spv"};
    if (!VkHelpers::LoadShaderModule(shaderPath.string().c_str(), context->device, &computeShader)) {
        LOG_ERROR("Failed to load {}", shaderPath.string());
        exit(1);
    }

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = VkHelpers::PipelineShaderStageCreateInfo(computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
    VkComputePipelineCreateInfo computePipelineCreateInfo = VkHelpers::ComputePipelineCreateInfo(pipelineLayout.handle, pipelineShaderStageCreateInfo);
    pipeline = VkResources::CreateComputePipeline(context, computePipelineCreateInfo);

    // Cleanup
    vkDestroyShaderModule(context->device, computeShader, nullptr);
}

InstancingPrefixSumPipeline::InstancingPrefixSumPipeline(InstancingPrefixSumPipeline&& other) noexcept
{
    pipelineLayout = std::move(other.pipelineLayout);
    pipeline = std::move(other.pipeline);
    context = other.context;
    other.context = nullptr;
}

InstancingPrefixSumPipeline& InstancingPrefixSumPipeline::operator=(InstancingPrefixSumPipeline&& other) noexcept
{
    if (this != &other) {
        pipelineLayout = std::move(other.pipelineLayout);
        pipeline = std::move(other.pipeline);
        context = other.context;
        other.context = nullptr;
    }
    return *this;
}
} // Renderer