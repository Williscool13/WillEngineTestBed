//
// Created by William on 2025-10-11.
//

#ifndef WILLENGINETESTBED_VK_RESOURCES_H
#define WILLENGINETESTBED_VK_RESOURCES_H

#include <volk/volk.h>
#include <vma/include/vk_mem_alloc.h>

#include "vk_context.h"

namespace Renderer
{
struct VulkanContext;


struct AllocatedBuffer
{
    VulkanContext* context{nullptr};

    VkBuffer handle{VK_NULL_HANDLE};
    VkDeviceAddress address{0};
    size_t size{0};

    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo allocationInfo{};

    AllocatedBuffer() = default;

    ~AllocatedBuffer();

    AllocatedBuffer(const AllocatedBuffer&) = delete;

    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

    AllocatedBuffer(AllocatedBuffer&& other) noexcept;

    AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept;
};

struct AllocatedImage
{
    VulkanContext* context{nullptr};

    VkImage handle{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent3D extent{};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    uint32_t mipLevels{0};
    VmaAllocation allocation{};

    AllocatedImage() = default;

    ~AllocatedImage();

    AllocatedImage(const AllocatedImage&) = delete;

    AllocatedImage& operator=(const AllocatedImage&) = delete;

    AllocatedImage(AllocatedImage&& other) noexcept;

    AllocatedImage& operator=(AllocatedImage&& other) noexcept;
};

struct ImageView
{
    VulkanContext* context{};

    VkImageView handle{};

    ImageView() = default;

    ~ImageView();

    ImageView(const ImageView&) = delete;

    ImageView& operator=(const ImageView&) = delete;

    ImageView(ImageView&& other) noexcept;

    ImageView& operator=(ImageView&& other) noexcept;
};

struct Sampler
{
    VulkanContext* context{};

    VkSampler handle{};

    Sampler() = default;

    ~Sampler();

    Sampler(const Sampler&) = delete;

    Sampler& operator=(const Sampler&) = delete;

    Sampler(Sampler&& other) noexcept;

    Sampler& operator=(Sampler&& other) noexcept;
};

struct DescriptorSetLayout
{
    VulkanContext* context{};
    VkDescriptorSetLayout handle{};

    DescriptorSetLayout() = default;

    ~DescriptorSetLayout();

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;

    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept;

    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept;
};

struct PipelineLayout
{
    VulkanContext* context{};
    VkPipelineLayout handle{};

    PipelineLayout() = default;

    ~PipelineLayout();

    PipelineLayout(const PipelineLayout&) = delete;

    PipelineLayout& operator=(const PipelineLayout&) = delete;

    PipelineLayout(PipelineLayout&& other) noexcept;

    PipelineLayout& operator=(PipelineLayout&& other) noexcept;
};

struct Pipeline
{
    VulkanContext* context{};
    VkPipeline handle{};

    Pipeline() = default;

    ~Pipeline();

    Pipeline(const Pipeline&) = delete;

    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline(Pipeline&& other) noexcept;

    Pipeline& operator=(Pipeline&& other) noexcept;
};


namespace VkResources
{
    AllocatedImage CreateAllocatedImage(VulkanContext* context, const VkImageCreateInfo& imageCreateInfo);

    ImageView CreateImageView(VulkanContext* context, const VkImageViewCreateInfo& imageViewCreateInfo);

    AllocatedBuffer CreateAllocatedBuffer(VulkanContext* context, const VkBufferCreateInfo& bufferInfo, const VmaAllocationCreateInfo& vmaAllocInfo);

    Sampler CreateSampler(VulkanContext* context, const VkSamplerCreateInfo& samplerCreateInfo);

    DescriptorSetLayout CreateDescriptorSetLayout(VulkanContext* context, const VkDescriptorSetLayoutCreateInfo& layoutCreateInfo);

    PipelineLayout CreatePipelineLayout(VulkanContext* context, const VkPipelineLayoutCreateInfo& layoutCreateInfo);

    Pipeline CreateGraphicsPipeline(VulkanContext* context, const VkGraphicsPipelineCreateInfo& pipelineCreateInfo);

    Pipeline CreateComputePipeline(VulkanContext* context, const VkComputePipelineCreateInfo& pipelineCreateInfo);
}
}

#endif //WILLENGINETESTBED_VK_RESOURCES_H
