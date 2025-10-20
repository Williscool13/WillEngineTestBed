//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_LOADER_H
#define WILLENGINETESTBED_MODEL_LOADER_H
#include <filesystem>

#include "fastgltf/types.hpp"
#include "model/model_data.h"

namespace Renderer
{
class ModelLoader
{
public:
    ModelLoader() = default;

    explicit ModelLoader(VulkanContext* context);

    ~ModelLoader() = default;



    ModelData LoadGltf(std::filesystem::path path);

    VkFilter ExtractFilter(fastgltf::Filter filter);

    VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter);

    AllocatedImage LoadGltfImage(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder);

    MaterialProperties ExtractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

    void LoadTextureIndicesAndUV(const fastgltf::TextureInfo& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex, glm::vec4& uvTransform);

    glm::vec4 GenerateBoundingSphere(const std::vector<VertexPosition>& vertices);

private:
    VulkanContext* context{};
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_LOADER_H
