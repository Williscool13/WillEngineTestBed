//
// Created by William on 2025-10-19.
//

#ifndef WILLENGINETESTBED_DESCRIPOR_BUFFER_BINDLESS_RESOURCES_H
#define WILLENGINETESTBED_DESCRIPOR_BUFFER_BINDLESS_RESOURCES_H
#include <span>
#include <vector>

#include "../vk_resources.h"

namespace Renderer
{
struct VulkanContext;

/*
 * Mega bindless resource buffer.
 *  - 2 bindings - sampler and sampled texture.
 *  - Single descriptor set.
 *  - Resource limits are defined in render_constants
 */
struct DescriptorBufferBindlessResources
{
public:
    DescriptorSetLayout descriptorSetLayout{};

public:
    DescriptorBufferBindlessResources();

    explicit DescriptorBufferBindlessResources(VulkanContext* context);

    ~DescriptorBufferBindlessResources();

    DescriptorBufferBindlessResources(const DescriptorBufferBindlessResources&) = delete;

    DescriptorBufferBindlessResources& operator=(const DescriptorBufferBindlessResources&) = delete;

    DescriptorBufferBindlessResources(DescriptorBufferBindlessResources&& other) noexcept;

    DescriptorBufferBindlessResources& operator=(DescriptorBufferBindlessResources&& other) noexcept;

    int32_t AllocateSampler(VkSampler sampler);

    int32_t AllocateTexture(const VkDescriptorImageInfo& imageInfo);

    void ReleaseSamplerBinding(int32_t bindingArrayIndex);

    void ReleaseTextureBinding(int32_t bindingArrayIndex);

    [[nodiscard]] VkDescriptorBufferBindingInfoEXT GetBindingInfo() const;

private:
    VulkanContext* context{};
    AllocatedBuffer buffer{};

    VkDeviceSize descriptorSetSize{};

    std::vector<int32_t> freeSamplerIndices;
    std::vector<int32_t> freeTextureIndices;
};
} // Renderer

#endif //WILLENGINETESTBED_DESCRIPOR_BUFFER_BINDLESS_RESOURCES_H
