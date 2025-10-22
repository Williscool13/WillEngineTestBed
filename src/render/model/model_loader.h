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
    VkFilter ExtractFilter(fastgltf::Filter filter);

    VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter);

    void LoadGltfImages(const fastgltf::Asset& asset, const std::filesystem::path& parentFolder, std::vector<AllocatedImage>& outAllocatedImages);

    MaterialProperties ExtractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

    void LoadTextureIndicesAndUV(const fastgltf::TextureInfo& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex, glm::vec4& uvTransform);

    glm::vec4 GenerateBoundingSphere(const std::vector<Vertex>& vertices);

    AllocatedImage RecordCreateImageFromData(VkCommandBuffer cmd, size_t offset, unsigned char* data, size_t size, VkExtent3D imageExtent, VkFormat format, VkImageUsageFlagBits usage, bool mipmapped);

private:
    VulkanContext* context{};

    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer{};
    VkFence uploadFence{};

    AllocatedBuffer imageStagingBuffer{};
    OffsetAllocator::Allocator imageStagingAllocator{IMAGE_UPLOAD_STAGING_SIZE};
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_LOADER_H
