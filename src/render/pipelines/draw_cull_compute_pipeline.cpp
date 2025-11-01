//
// Created by William on 2025-11-01.
//

#include "draw_cull_compute_pipeline.h"

#include <filesystem>

#include "crash-handling/logger.h"
#include "render/vk_helpers.h"
#include "render/vk_types.h"

namespace Renderer
{
DrawCullComputePipeline::DrawCullComputePipeline() = default;

DrawCullComputePipeline::~DrawCullComputePipeline() = default;

DrawCullComputePipeline::DrawCullComputePipeline(VulkanContext* context)
    : context(context)
{
    VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};
    computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutCreateInfo.pNext = nullptr;
    computePipelineLayoutCreateInfo.pSetLayouts = nullptr;
    computePipelineLayoutCreateInfo.setLayoutCount = 0;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(BindlessIndirectPushConstant);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
    computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    drawCullPipelineLayout = VkResources::CreatePipelineLayout(context, computePipelineLayoutCreateInfo);

    VkShaderModule computeShader;
    std::filesystem::path shaderPath = {"shaders/drawCull_compute.spv"};
    if (!VkHelpers::LoadShaderModule(shaderPath.string().c_str(), context->device, &computeShader)) {
        LOG_ERROR("Failed to load {}", shaderPath.string());
        exit(1);
    }

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = VkHelpers::PipelineShaderStageCreateInfo(computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
    VkComputePipelineCreateInfo computePipelineCreateInfo = VkHelpers::ComputePipelineCreateInfo(drawCullPipelineLayout.handle, pipelineShaderStageCreateInfo);
    drawCullPipeline = VkResources::CreateComputePipeline(context, computePipelineCreateInfo);

    // Cleanup
    vkDestroyShaderModule(context->device, computeShader, nullptr);
}

DrawCullComputePipeline::DrawCullComputePipeline(DrawCullComputePipeline&& other) noexcept
{
    drawCullPipelineLayout = std::move(other.drawCullPipelineLayout);
    drawCullPipeline = std::move(other.drawCullPipeline);
    context = other.context;
    other.context = nullptr;
}

DrawCullComputePipeline& DrawCullComputePipeline::operator=(DrawCullComputePipeline&& other) noexcept
{
    if (this != &other) {
        drawCullPipelineLayout = std::move(other.drawCullPipelineLayout);
        drawCullPipeline = std::move(other.drawCullPipeline);
        context = other.context;
        other.context = nullptr;
    }
    return *this;
}
} // Renderer
