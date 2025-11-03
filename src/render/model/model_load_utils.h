//
// Created by William on 2025-11-03.
//

#ifndef WILLENGINETESTBED_MODEL_LOAD_UTILS_H
#define WILLENGINETESTBED_MODEL_LOAD_UTILS_H

#include <volk/volk.h>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>

#include "render/vk_types.h"


namespace Renderer::ModelLoadUtils
{
VkFilter ExtractFilter(fastgltf::Filter filter);

VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter);

MaterialProperties ExtractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

void LoadTextureIndicesAndUV(const fastgltf::TextureInfo& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex, glm::vec4& uvTransform);

glm::vec4 GenerateBoundingSphere(const std::vector<Vertex>& vertices);
;
} // Renderer

#endif //WILLENGINETESTBED_MODEL_LOAD_UTILS_H
