//
// Created by William on 2025-10-20.
//

#include "model_loader.h"

#include "fastgltf/core.hpp"
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "utils/utils.h"

namespace Renderer
{
ModelLoader::ModelLoader(VulkanContext* context)
    : context(context)
{}

ModelData ModelLoader::LoadGltf(std::filesystem::path path)
{
    ModelData model{};

    //
    {
        Utils::ScopedTimer timer{fmt::format("Loading {}", path.filename().string())};

        fastgltf::Parser parser{fastgltf::Extensions::KHR_texture_basisu | fastgltf::Extensions::KHR_mesh_quantization | fastgltf::Extensions::KHR_texture_transform};
        constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
                                     | fastgltf::Options::AllowDouble
                                     | fastgltf::Options::LoadExternalBuffers
                                     | fastgltf::Options::LoadExternalImages;

        auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
        if (!static_cast<bool>(gltfFile)) {
            LOG_ERROR("Failed to open glTF file ({}): {}\n", path.filename().string(), getErrorMessage(gltfFile.error()));
            return model;
        }

        auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
        if (!load) {
            LOG_ERROR("Failed to load glTF: {}\n", to_underlying(load.error()));
            return model;
        }

        fastgltf::Asset gltf = std::move(load.get());

        model.samplers.reserve(gltf.samplers.size());
        for (const fastgltf::Sampler& gltfSampler : gltf.samplers) {
            VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
            samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
            samplerInfo.minLod = 0;

            samplerInfo.magFilter = ExtractFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
            samplerInfo.minFilter = ExtractFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));


            samplerInfo.mipmapMode = ExtractMipmapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Linear));

            model.samplers.push_back(VkResources::CreateSampler(context, samplerInfo));
        }

        model.images.reserve(gltf.images.size());
        for (const fastgltf::Image& gltfImage : gltf.images) {
            model.images.push_back(LoadGltfImage(gltf, gltfImage, path.parent_path()));
        }

        // for (auto image : model.images) {
        //     imageViews
        // }
    }

    return model;
}

VkFilter ModelLoader::ExtractFilter(fastgltf::Filter filter)
{
    switch (filter) {
        // nearest samplers
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;
        // linear samplers
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode ModelLoader::ExtractMipmapMode(fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

AllocatedImage ModelLoader::LoadGltfImage(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder)
{
    return {};
}
} // Renderer
