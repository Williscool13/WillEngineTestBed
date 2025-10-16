//
// Created by William on 2025-10-11.
//

#ifndef WILLENGINETESTBED_DESCRIPTOR_BUFFER_STORAGE_H
#define WILLENGINETESTBED_DESCRIPTOR_BUFFER_STORAGE_H

#include <span>
#include <vector>

#include "../vk_resources.h"

namespace Renderer
{
struct DescriptorBufferStorage
{
    DescriptorBufferStorage();

    explicit DescriptorBufferStorage(VulkanContext* context, VkDescriptorSetLayout setLayout, int32_t maxSetCount = 3);

    ~DescriptorBufferStorage();

    void ReleaseDescriptorSet(int32_t descriptorSetIndex);

    void ReleaseAllDescriptorSets();

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer.
     * @return Index of allocated descriptor set for binding/releasing. Returns -1 if allocation fails.
     */
    int32_t AllocateDescriptorSet();

    /**
     * Updates all bindings in a descriptor set.
     * @param storageBuffers Buffers to bind. Must match descriptor set layout binding count.
     * @param descriptorSetIndex Index of descriptor set to update.
     * @param descriptorBindingIndex Index of the binding in the descriptor set
     * @return True if successful, false if index is invalid or not allocated.
     */
    bool UpdateDescriptorSet(std::span<AllocatedBuffer> storageBuffers, int32_t descriptorSetIndex, int32_t descriptorBindingIndex);

    /**
     * Updates a single binding in a descriptor set.
     * @param storageBuffer Buffer to bind.
     * @param descriptorSetIndex Index of descriptor set to update.
     * @param descriptorBindingIndex Index of the binding in the descriptor set
     * @param bindingArrayIndex Binding index within the descriptor set. Must be valid for the layout.
     * @return True if successful, false if indices are invalid or set not allocated.
     */
    bool UpdateDescriptor(const AllocatedBuffer& storageBuffer, int32_t descriptorSetIndex, int32_t descriptorBindingIndex, int32_t bindingArrayIndex);

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
}


#endif //WILLENGINETESTBED_DESCRIPTOR_BUFFER_STORAGE_H
