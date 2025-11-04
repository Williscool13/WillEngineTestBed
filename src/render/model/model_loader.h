//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_LOADER_H
#define WILLENGINETESTBED_MODEL_LOADER_H
#include <filesystem>

#include "fastgltf/types.hpp"
#include "model_data.h"
#include "render/render_constants.h"

namespace Renderer
{
class ModelLoader
{
public:
    ModelLoader() = default;

    explicit ModelLoader(VulkanContext* context);

    ~ModelLoader();

    ExtractedModel LoadGltf(const std::filesystem::path& path);

private:
    void LoadGltfImages(const fastgltf::Asset& asset, const std::filesystem::path& parentFolder, std::vector<AllocatedImage>& outAllocatedImages);

    AllocatedImage RecordCreateImageFromData(VkCommandBuffer cmd, size_t offset, unsigned char* data, size_t size, VkExtent3D imageExtent, VkFormat format, VkImageUsageFlagBits usage, bool mipmapped);

    void TopologicalSortNodes(std::vector<Node>& nodes, std::vector<uint32_t>& oldToNew);

private:
    VulkanContext* context{};

    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer{};
    VkFence uploadFence{};

    AllocatedBuffer imageStagingBuffer{};
    OffsetAllocator::Allocator imageStagingAllocator{STAGING_BUFFER_SIZE};

    std::vector<Node> sortedNodes;
    std::vector<bool> visited;
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_LOADER_H
