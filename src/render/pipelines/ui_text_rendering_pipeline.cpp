//
// Created by William on 2025-12-02.
//

#include "ui_text_rendering_pipeline.h"

#include <stdexcept>

#include "render/render_constants.h"
#include "render/vk_helpers.h"
#include "render/vk_pipelines.h"
#include "render/vk_types.h"

namespace Renderer
{
UITextRenderingPipeline::UITextRenderingPipeline() = default;

UITextRenderingPipeline::~UITextRenderingPipeline() = default;

UITextRenderingPipeline::UITextRenderingPipeline(VulkanContext* context, VkDescriptorSetLayout fontAtlasDescriptorSet, VkDescriptorSetLayout bindlessDescriptorSet) : context(context)
{
    VkPushConstantRange renderPushConstantRange{};
    renderPushConstantRange.offset = 0;
    renderPushConstantRange.size = sizeof(TextRenderingPushConstant);
    renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo renderPipelineLayoutCreateInfo{};
    renderPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout layouts[2]{fontAtlasDescriptorSet, bindlessDescriptorSet};
    renderPipelineLayoutCreateInfo.pSetLayouts = layouts;
    renderPipelineLayoutCreateInfo.setLayoutCount = 2;
    renderPipelineLayoutCreateInfo.pPushConstantRanges = &renderPushConstantRange;
    renderPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    pipelineLayout = VkResources::CreatePipelineLayout(context, renderPipelineLayoutCreateInfo);

    VkShaderModule vertShader;
    VkShaderModule fragShader;
    if (!VkHelpers::LoadShaderModule("shaders\\uiTextRendering_vertex.spv", context->device, &vertShader)) {
        throw std::runtime_error("Error when building the vertex shader (indirectDraw_vertex.spv)");
    }
    if (!VkHelpers::LoadShaderModule("shaders\\uiTextRendering_fragment.spv", context->device, &fragShader)) {
        throw std::runtime_error("Error when building the fragment shader (indirectDraw_fragment.spv)");
    }


    RenderPipelineBuilder renderPipelineBuilder;
    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        {
            .binding = 0,
            .stride = sizeof(UIVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(UIVertex, position),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(UIVertex, uv),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(UIVertex, color),
        },
        {
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(UIVertex, textureIndex),
        },
        {
            .location = 4,
            .binding = 0,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(UIVertex, bIsText),
        },
    };

    renderPipelineBuilder.SetupVertexInput(vertexBindings, vertexAttributes);
    renderPipelineBuilder.SetShaders(vertShader, fragShader);

    VkPipelineColorBlendAttachmentState blendState{};
    blendState.blendEnable = VK_TRUE;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    renderPipelineBuilder.SetupBlending({blendState});

    renderPipelineBuilder.SetupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.SetupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    renderPipelineBuilder.DisableMultisampling();
    renderPipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.SetupRenderer({DRAW_IMAGE_FORMAT}, VK_FORMAT_D32_SFLOAT);
    renderPipelineBuilder.SetupPipelineLayout(pipelineLayout.handle);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.GeneratePipelineCreateInfo();
    pipeline = VkResources::CreateGraphicsPipeline(context, pipelineCreateInfo);

    vkDestroyShaderModule(context->device, vertShader, nullptr);
    vkDestroyShaderModule(context->device, fragShader, nullptr);
}

UITextRenderingPipeline::UITextRenderingPipeline(UITextRenderingPipeline&& other) noexcept
{
    pipelineLayout = std::move(other.pipelineLayout);
    pipeline = std::move(other.pipeline);
    context = other.context;
    other.context = nullptr;
}

UITextRenderingPipeline& UITextRenderingPipeline::operator=(UITextRenderingPipeline&& other) noexcept
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
