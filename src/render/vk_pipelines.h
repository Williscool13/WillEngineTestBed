//
// Created by William on 2025-10-12.
//

#ifndef WILLENGINETESTBED_VK_PIPELINESA_H
#define WILLENGINETESTBED_VK_PIPELINESA_H

#include <vector>
#include <volk/volk.h>

namespace Renderer
{
class RenderPipelineBuilder
{
public:
    enum class BlendMode
    {
        ALPHA_BLEND,
        ADDITIVE_BLEND,
        NO_BLEND
    };

    RenderPipelineBuilder();

    VkGraphicsPipelineCreateInfo generatePipelineCreateInfo(VkPipelineCreateFlagBits flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    void clear();

    void setShaders(VkShaderModule vertexShader);

    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setShaders(VkShaderModule vertexShader, VkShaderModule tessControlShader, VkShaderModule tessEvalShader, VkShaderModule fragmentShader);

    void setupVertexInput(const std::vector<VkVertexInputBindingDescription>& bindings, const std::vector<VkVertexInputAttributeDescription>& attributes);

    void setupInputAssembly(VkPrimitiveTopology topology, bool enablePrimitiveRestart = false);

    void setupRasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace, float lineWidth = 1.0f, bool rasterizerDiscardEnable = false);

    void enableDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);

    void setupMultisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples, float minSampleShading, const VkSampleMask* pSampleMask, VkBool32 alphaToCoverageEnable,
                            VkBool32 alphaToOneEnable);

    /**
     * Shortcut to disable multisampling for this pipeline
     */
    void disableMultisampling();

    void setupRenderer(const std::vector<VkFormat>& colorAttachmentFormat, VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED, VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED);

    void setupDepthStencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp, VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable, const VkStencilOpState& front,
                           const VkStencilOpState& back, float minDepthBounds, float maxDepthBounds);

    void enableDepthTest(VkBool32 depthWriteEnable, VkCompareOp op);

    void disableDepthTest();

    void setupBlending(const std::vector<VkPipelineColorBlendAttachmentState>& blendAttachmentStates_);

    void setupPipelineLayout(VkPipelineLayout pipelineLayout_);

    void setupTessellation(int32_t controlPoints = 4);

    void addDynamicState(VkDynamicState dynamicState);

private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    // Viewport, details not necessary here (dynamic rendering)
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 0,
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineRenderingCreateInfo renderInfo{};
    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = 0,
        .pDynamicStates = nullptr,
    };

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};


    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;

    std::vector<VkFormat> colorAttachmentFormats;

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates{};

    bool bIsTessellationEnabled{false};
    VkPipelineTessellationStateCreateInfo tessellation{};


    std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
};
}


#endif //WILLENGINETESTBED_VK_PIPELINESA_H
