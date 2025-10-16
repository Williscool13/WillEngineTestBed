//
// Created by William on 2025-10-11.
//

#ifndef WILLENGINETESTBED_DESCRIPTOR_BUFFER_STORAGE_IMAGE_H
#define WILLENGINETESTBED_DESCRIPTOR_BUFFER_STORAGE_IMAGE_H

#include <span>
#include <vector>

#include "../vk_resources.h"

namespace Renderer
{
struct VulkanContext;

struct DescriptorBufferStorageImage
{
    DescriptorBufferStorageImage();

    explicit DescriptorBufferStorageImage(VulkanContext* context, VkDescriptorSetLayout setLayout, int32_t maxSetCount = 3);

    ~DescriptorBufferStorageImage();

    void ReleaseDescriptorSet(int32_t descriptorSetIndex);

    void ReleaseAllDescriptorSets();

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer.
     * @return Index of allocated descriptor set for binding/releasing. Returns -1 if allocation fails.
     */
    int32_t AllocateDescriptorSet();

    /**
     * Updates all bindings in a descriptor set.
     * @param imageInfos Image info to bind. Must match descriptor set layout binding count.
     * @param descriptorSetIndex Index of descriptor set to update.
     * @param descriptorBindingIndex Index of the binding in the descriptor
     * @return True if successful, false if index is invalid or not allocated.
     */
    bool UpdateDescriptorSet(std::span<VkDescriptorImageInfo> imageInfos, int32_t descriptorSetIndex, int32_t descriptorBindingIndex);

    /**
     * Updates a single binding in a descriptor set.
     * @param imageInfo Image info to bind.
     * @param descriptorSetIndex Index of descriptor set to update.
     * @param descriptorBindingIndex Index of the binding in the descriptor set
     * @param bindingArrayIndex Binding index within the descriptor set. Must be valid for the layout.
     * @return True if successful, false if indices are invalid or set not allocated.
     */
    bool UpdateDescriptor(const VkDescriptorImageInfo& imageInfo, int32_t descriptorSetIndex, int32_t descriptorBindingIndex, int32_t bindingArrayIndex);

    [[nodiscard]] VkDescriptorBufferBindingInfoEXT GetBindingInfo() const;

    [[nodiscard]] VkDeviceSize GetOffset(const int32_t descriptorSetIndex) const { return descriptorSetSize * descriptorSetIndex; }

private:
    VulkanContext* context{};
    AllocatedBuffer buffer{};
    VkDescriptorSetLayout descriptorSetLayout{};

    /**
     * The size of 1 descriptor set
     */
    int32_t maxDescriptorSets{};

    VkDeviceSize descriptorSetSize{};

    std::vector<int32_t> freeIndices;
};

} // Renderer

#endif //WILLENGINETESTBED_DESCRIPTOR_BUFFER_STORAGE_IMAGE_H