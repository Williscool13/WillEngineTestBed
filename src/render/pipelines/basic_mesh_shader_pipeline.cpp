//
// Created by William on 2025-11-18.
//

#include "basic_mesh_shader_pipeline.h"

#include <stdexcept>

#include "render/render_constants.h"
#include "render/vk_helpers.h"
#include "render/vk_pipelines.h"
#include "render/vk_types.h"

namespace Renderer
{
BasicMeshShaderPipeline::BasicMeshShaderPipeline() = default;

BasicMeshShaderPipeline::~BasicMeshShaderPipeline() = default;

BasicMeshShaderPipeline::BasicMeshShaderPipeline(VulkanContext* context) : context(context)
{
    VkPushConstantRange renderPushConstantRange{};
    renderPushConstantRange.offset = 0;
    renderPushConstantRange.size = sizeof(BasicMeshShaderPushConstants);
    renderPushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo BasicMeshShaderPipelineLayoutCreateInfo{};
    BasicMeshShaderPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    BasicMeshShaderPipelineLayoutCreateInfo.pSetLayouts = nullptr;
    BasicMeshShaderPipelineLayoutCreateInfo.setLayoutCount = 0;
    BasicMeshShaderPipelineLayoutCreateInfo.pPushConstantRanges = &renderPushConstantRange;
    BasicMeshShaderPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    pipelineLayout = VkResources::CreatePipelineLayout(context, BasicMeshShaderPipelineLayoutCreateInfo);

    VkShaderModule taskShader;
    VkShaderModule meshShader;
    VkShaderModule fragmentShader;
    if (!VkHelpers::LoadShaderModule("shaders\\basicMeshShader_task.spv", context->device, &taskShader)) {
        throw std::runtime_error("Error when building the vertex shader (basicMeshShader_task.spv)");
    }
    if (!VkHelpers::LoadShaderModule("shaders\\basicMeshShader_mesh.spv", context->device, &meshShader)) {
        throw std::runtime_error("Error when building the fragment shader (basicMeshShader_mesh.spv)");
    }
    if (!VkHelpers::LoadShaderModule("shaders\\basicMeshShader_fragment.spv", context->device, &fragmentShader)) {
        throw std::runtime_error("Error when building the fragment shader (basicMeshShader_fragment.spv)");
    }

    RenderPipelineBuilder pipelineBuilder;

    pipelineBuilder.SetTaskMeshShaders(taskShader, meshShader, fragmentShader);
    pipelineBuilder.SetupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.SetupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.DisableMultisampling();
    pipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.SetupRenderer({DRAW_IMAGE_FORMAT}, DEPTH_IMAGE_FORMAT);
    pipelineBuilder.SetupPipelineLayout(pipelineLayout.handle);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = pipelineBuilder.GeneratePipelineCreateInfo();
    pipeline = VkResources::CreateGraphicsPipeline(context, pipelineCreateInfo);

    vkDestroyShaderModule(context->device, taskShader, nullptr);
    vkDestroyShaderModule(context->device, meshShader, nullptr);
    vkDestroyShaderModule(context->device, fragmentShader, nullptr);
}

BasicMeshShaderPipeline::BasicMeshShaderPipeline(BasicMeshShaderPipeline&& other) noexcept
{
    pipelineLayout = std::move(other.pipelineLayout);
    pipeline = std::move(other.pipeline);
    context = other.context;
    other.context = nullptr;
}

BasicMeshShaderPipeline& BasicMeshShaderPipeline::operator=(BasicMeshShaderPipeline&& other) noexcept
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
