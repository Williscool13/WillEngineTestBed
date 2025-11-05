//
// Created by William on 2025-11-05.
//

#include "main_render_pipeline.h"

#include <filesystem>

#include "render/render_constants.h"
#include "render/vk_helpers.h"
#include "render/vk_pipelines.h"
#include "render/vk_types.h"

namespace Renderer
{
MainRenderPipeline::MainRenderPipeline() = default;

MainRenderPipeline::~MainRenderPipeline() = default;

MainRenderPipeline::MainRenderPipeline(VulkanContext* context, VkDescriptorSetLayout bindlessDescriptorSet) : context(context)
{
    VkPushConstantRange renderPushConstantRange{};
    renderPushConstantRange.offset = 0;
    renderPushConstantRange.size = sizeof(BindlessAddressPushConstant);
    renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo renderPipelineLayoutCreateInfo{};
    renderPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    renderPipelineLayoutCreateInfo.pSetLayouts = &bindlessDescriptorSet;
    renderPipelineLayoutCreateInfo.setLayoutCount = 1;
    renderPipelineLayoutCreateInfo.pPushConstantRanges = &renderPushConstantRange;
    renderPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    renderPipelineLayout = VkResources::CreatePipelineLayout(context, renderPipelineLayoutCreateInfo);

    VkShaderModule vertShader;
    VkShaderModule fragShader;
    if (!VkHelpers::LoadShaderModule("shaders\\indirectDraw_vertex.spv", context->device, &vertShader)) {
        throw std::runtime_error("Error when building the vertex shader (indirectDraw_vertex.spv)");
    }
    if (!VkHelpers::LoadShaderModule("shaders\\indirectDraw_fragment.spv", context->device, &fragShader)) {
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

    renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({DRAW_IMAGE_FORMAT}, DEPTH_IMAGE_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(renderPipelineLayout.handle);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
    renderPipeline = VkResources::CreateGraphicsPipeline(context, pipelineCreateInfo);

    vkDestroyShaderModule(context->device, vertShader, nullptr);
    vkDestroyShaderModule(context->device, fragShader, nullptr);
}

MainRenderPipeline::MainRenderPipeline(MainRenderPipeline&& other) noexcept
{
    renderPipelineLayout = std::move(other.renderPipelineLayout);
    renderPipeline = std::move(other.renderPipeline);
    context = other.context;
    other.context = nullptr;
}

MainRenderPipeline& MainRenderPipeline::operator=(MainRenderPipeline&& other) noexcept
{
    if (this != &other) {
        renderPipelineLayout = std::move(other.renderPipelineLayout);
        renderPipeline = std::move(other.renderPipeline);
        context = other.context;
        other.context = nullptr;
    }
    return *this;
}
} // Renderer
