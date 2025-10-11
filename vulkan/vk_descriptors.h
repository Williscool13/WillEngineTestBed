//
// Created by William on 8/11/2024.
// This class contains structs and classes that help with management of descriptors. THis includes descriptor layouts and buffers.
//

#ifndef VKDESCRIPTORS_H
#define VKDESCRIPTORS_H
#include <vector>
#include <volk/volk.h>


struct DescriptorLayoutBuilder
{
    explicit DescriptorLayoutBuilder(uint32_t reservedSize = 0);

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void AddBinding(uint32_t binding, VkDescriptorType type);

    void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count);

    void Clear();

    VkDescriptorSetLayoutCreateInfo Build(VkShaderStageFlagBits shaderStageFlags, VkDescriptorSetLayoutCreateFlags layoutCreateFlags);

    VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};


#endif //VKDESCRIPTORS_H
