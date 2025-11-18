//
// Created by William on 2025-11-18.
//

#include "main_mesh_shader_pipeline.h"

#include <stdexcept>

#include "render/render_constants.h"
#include "render/vk_helpers.h"
#include "render/vk_pipelines.h"
#include "render/vk_types.h"


namespace Renderer
{
MainMeshShaderPipeline::MainMeshShaderPipeline() = default;

MainMeshShaderPipeline::~MainMeshShaderPipeline() = default;

MainMeshShaderPipeline::MainMeshShaderPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet) : context(context)
{
    VkPushConstantRange renderPushConstantRange{};
    renderPushConstantRange.offset = 0;
    renderPushConstantRange.size = sizeof(MainMeshShaderPipeline);
    renderPushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pSetLayouts = &bindlessDescriptorSet;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &renderPushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    pipelineLayout = VkResources::CreatePipelineLayout(context, pipelineLayoutCreateInfo);

    VkShaderModule taskShader;
    VkShaderModule meshShader;
    VkShaderModule fragmentShader;
    if (!VkHelpers::LoadShaderModule("shaders\\meshShader_task.spv", context->device, &taskShader)) {
        throw std::runtime_error("Error when building the vertex shader (meshShader_task.spv)");
    }
    if (!VkHelpers::LoadShaderModule("shaders\\meshShader_mesh.spv", context->device, &meshShader)) {
        throw std::runtime_error("Error when building the fragment shader (meshShader_mesh.spv)");
    }
    if (!VkHelpers::LoadShaderModule("shaders\\meshShader_fragment.spv", context->device, &fragmentShader)) {
        throw std::runtime_error("Error when building the fragment shader (meshShader_fragment.spv)");
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

MainMeshShaderPipeline::MainMeshShaderPipeline(MainMeshShaderPipeline&& other) noexcept
{
    pipelineLayout = std::move(other.pipelineLayout);
    pipeline = std::move(other.pipeline);
    context = other.context;
    other.context = nullptr;
}

MainMeshShaderPipeline& MainMeshShaderPipeline::operator=(MainMeshShaderPipeline&& other) noexcept
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