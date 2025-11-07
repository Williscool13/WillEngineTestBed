//
// Created by William on 2025-11-06.
//

#ifndef WILLENGINETESTBED_RENDER_OPERATIONS_H
#define WILLENGINETESTBED_RENDER_OPERATIONS_H

#include <glm/glm.hpp>

#include "../model/model_data.h"

namespace Renderer
{
struct ModelMatrixOperation
{
    uint32_t index{};
    glm::mat4 modelMatrix{1.0f};

    // Filled and used by render thread
    uint32_t frames{};
};

struct InstanceOperation
{
    uint32_t index{};
    Instance instance{};

    // Filled and used by render thread
    uint32_t frames{};
};

struct JointMatrixOperation
{
    uint32_t index{};
    glm::mat4 jointMatrix{};

    // Filled and used by render thread
    uint32_t frames{};
};

struct BufferAcquireOperation
{
    VkPipelineStageFlags2 srcStageMask;
    VkAccessFlags2 srcAccessMask;
    VkPipelineStageFlags2 dstStageMask;
    VkAccessFlags2 dstAccessMask;
    uint32_t srcQueueFamilyIndex;

    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
};

struct ImageAcquireOperation
{
    VkPipelineStageFlags2 srcStageMask;
    VkAccessFlags2 srcAccessMask;
    VkImageLayout oldLayout;
    VkPipelineStageFlags2 dstStageMask;
    VkAccessFlags2 dstAccessMask;
    VkImageLayout newLayout;
    uint32_t srcQueueFamilyIndex;

    VkImage image;
    VkImageSubresourceRange subresourceRange;
};

struct FrameBuffer
{
    RawSceneData rawSceneData{};
    uint64_t currentFrame{};

    std::vector<BufferAcquireOperation> bufferAcquireOperations;
    std::vector<ImageAcquireOperation> imageAcquireOperations;

    std::vector<ModelMatrixOperation> modelMatrixOperations;
    std::vector<InstanceOperation> instanceOperations;
    std::vector<JointMatrixOperation> jointMatrixOperations;
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_OPERATIONS_H
