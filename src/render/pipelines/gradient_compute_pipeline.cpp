//
// Created by William on 2025-11-01.
//

#include "gradient_compute_pipeline.h"

#include <filesystem>

#include "crash-handling/logger.h"
#include "render/vk_helpers.h"


namespace Renderer
{
GradientComputePipeline::GradientComputePipeline() = default;

GradientComputePipeline::~GradientComputePipeline() = default;

GradientComputePipeline::GradientComputePipeline(VulkanContext* context, VkDescriptorSetLayout renderTargetSetLayout)
    : context(context)
{
    VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};
    computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutCreateInfo.pNext = nullptr;
    computePipelineLayoutCreateInfo.pSetLayouts = &renderTargetSetLayout;
    computePipelineLayoutCreateInfo.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(int32_t) * 2;
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
    computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    gradientPipelineLayout = VkResources::CreatePipelineLayout(context, computePipelineLayoutCreateInfo);

    VkShaderModule computeShader;
    std::filesystem::path shaderPath = {"shaders/compute_compute.spv"};
    if (!VkHelpers::LoadShaderModule(shaderPath.string().c_str(), context->device, &computeShader)) {
        LOG_ERROR("Failed to load {}", shaderPath.string());
        exit(1);
    }

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = VkHelpers::PipelineShaderStageCreateInfo(computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
    VkComputePipelineCreateInfo computePipelineCreateInfo = VkHelpers::ComputePipelineCreateInfo(gradientPipelineLayout.handle, pipelineShaderStageCreateInfo);
    gradientPipeline = VkResources::CreateComputePipeline(context, computePipelineCreateInfo);

    // Cleanup
    vkDestroyShaderModule(context->device, computeShader, nullptr);
}

GradientComputePipeline::GradientComputePipeline(GradientComputePipeline&& other) noexcept
{
    gradientPipelineLayout = std::move(other.gradientPipelineLayout);
    gradientPipeline = std::move(other.gradientPipeline);
    context = other.context;
    other.context = nullptr;
}

GradientComputePipeline& GradientComputePipeline::operator=(GradientComputePipeline&& other) noexcept
{
    if (this != &other) {
        gradientPipelineLayout = std::move(other.gradientPipelineLayout);
        gradientPipeline = std::move(other.gradientPipeline);
        context = other.context;
        other.context = nullptr;
    }
    return *this;
}
} // Renderer
