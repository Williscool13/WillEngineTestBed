//
// Created by William on 2025-11-17.
//

#include "render_pipeline.h"

#include <stdexcept>

#include "render/render_constants.h"
#include "render/vk_helpers.h"
#include "render/vk_pipelines.h"
#include "render/vk_types.h"

namespace Renderer
{
RenderPipeline::RenderPipeline() = default;

RenderPipeline::~RenderPipeline() = default;

RenderPipeline::RenderPipeline(VulkanContext* context) : context(context)
{
    VkPushConstantRange renderPushConstantRange{};
    renderPushConstantRange.offset = 0;
    renderPushConstantRange.size = sizeof(RenderPushConstants);
    renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo renderPipelineLayoutCreateInfo{};
    renderPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    renderPipelineLayoutCreateInfo.pSetLayouts = nullptr;
    renderPipelineLayoutCreateInfo.setLayoutCount = 0;
    renderPipelineLayoutCreateInfo.pPushConstantRanges = &renderPushConstantRange;
    renderPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    pipelineLayout = VkResources::CreatePipelineLayout(context, renderPipelineLayoutCreateInfo);

    VkShaderModule vertShader;
    VkShaderModule fragShader;
    if (!VkHelpers::LoadShaderModule("shaders\\render2_vertex.spv", context->device, &vertShader)) {
        throw std::runtime_error("Error when building the vertex shader (indirectDraw_vertex.spv)");
    }
    if (!VkHelpers::LoadShaderModule("shaders\\render2_fragment.spv", context->device, &fragShader)) {
        throw std::runtime_error("Error when building the fragment shader (indirectDraw_fragment.spv)");
    }


    RenderPipelineBuilder renderPipelineBuilder;

    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        {
            .binding = 1,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position),
        },
        {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
        },
        {
            .location = 2,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex, tangent),
        },
        {
            .location = 3,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex, color),
        },
        {
            .location = 4,
            .binding = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, uv),
        },
    };

    renderPipelineBuilder.SetupVertexInput(vertexBindings, vertexAttributes);

    renderPipelineBuilder.SetShaders(vertShader, fragShader);
    renderPipelineBuilder.SetupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.SetupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    renderPipelineBuilder.DisableMultisampling();
    renderPipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.SetupRenderer({DRAW_IMAGE_FORMAT}, DEPTH_IMAGE_FORMAT);
    renderPipelineBuilder.SetupPipelineLayout(pipelineLayout.handle);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.GeneratePipelineCreateInfo();
    pipeline = VkResources::CreateGraphicsPipeline(context, pipelineCreateInfo);

    vkDestroyShaderModule(context->device, vertShader, nullptr);
    vkDestroyShaderModule(context->device, fragShader, nullptr);
}

RenderPipeline::RenderPipeline(RenderPipeline&& other) noexcept
{
    pipelineLayout = std::move(other.pipelineLayout);
    pipeline = std::move(other.pipeline);
    context = other.context;
    other.context = nullptr;
}

RenderPipeline& RenderPipeline::operator=(RenderPipeline&& other) noexcept
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
