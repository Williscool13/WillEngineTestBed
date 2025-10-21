//
// Created by William on 2025-10-20.
//

#include "model_loader.h"

#include "fastgltf/core.hpp"
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <glm/glm.hpp>

#include "render_utils.h"
#include "vk_helpers.h"
#include "crash-handling/crash_handler.h"
#include "stb/stb_image.h"
#include "utils/utils.h"

namespace Renderer
{
ModelLoader::ModelLoader(VulkanContext* context)
    : context(context)
{
    VkCommandPoolCreateInfo poolInfo = VkHelpers::CommandPoolCreateInfo(context->graphicsQueueFamily);
    VK_CHECK(vkCreateCommandPool(context->device, &poolInfo, nullptr, &commandPool));
    VkCommandBufferAllocateInfo cmdInfo = VkHelpers::CommandBufferAllocateInfo(1, commandPool);
    VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdInfo, &commandBuffer));


    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0, // Unsignaled
    };
    VK_CHECK(vkCreateFence(context->device, &fenceInfo, nullptr, &uploadFence));
}

ModelLoader::~ModelLoader()
{
    if (context && commandPool != VK_NULL_HANDLE) {
        // Command buffer is freed when pool is destroyed.
        vkDestroyCommandPool(context->device, commandPool, nullptr);
        vkDestroyFence(context->device, uploadFence, nullptr);
    }
}

ModelData ModelLoader::LoadGltf(const std::filesystem::path& path)
{
    ModelData model{};

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
        // todo: job system to parallelize stb decoding.
        // todo: batch the image upload on the GPU, so we only have to call the fence once.
        model.images.push_back(LoadGltfImage(gltf, gltfImage, path.parent_path()));
    }

    model.imageViews.reserve(gltf.images.size());
    for (const AllocatedImage& image : model.images) {
        VkImageViewCreateInfo imageViewCreateInfo = VkHelpers::ImageViewCreateInfo(image.handle, image.format, VK_IMAGE_ASPECT_COLOR_BIT);
        model.imageViews.push_back(VkResources::CreateImageView(context, imageViewCreateInfo));
    }

    model.materials.reserve(gltf.materials.size());

    for (const fastgltf::Material& gltfMaterial : gltf.materials) {
        MaterialProperties material = ExtractMaterial(gltf, gltfMaterial);
        model.materials.push_back(material);
    }

    std::vector<VertexPosition> primitiveVertexPositions{};
    std::vector<VertexProperty> primitiveVertexProperties{};
    std::vector<uint32_t> primitiveIndices{};

    model.meshes.reserve(gltf.meshes.size());
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshInformation meshData{};
        meshData.name = mesh.name;
        meshData.primitiveIndices.reserve(mesh.primitives.size());
        model.primitives.reserve(model.primitives.size() + mesh.primitives.size());

        for (fastgltf::Primitive& p : mesh.primitives) {
            Primitive primitiveData{};

            if (p.materialIndex.has_value()) {
                primitiveData.materialIndex = p.materialIndex.value();
                primitiveData.bHasTransparent = (static_cast<MaterialType>(model.materials[primitiveData.materialIndex].alphaProperties.y) == MaterialType::TRANSPARENT_);
            }

            // INDICES
            const fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
            primitiveIndices.clear();
            primitiveIndices.reserve(indexAccessor.count);

            fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](const std::uint32_t idx) {
                primitiveIndices.push_back(idx);
            });

            // POSITION (REQUIRED)
            const fastgltf::Attribute* positionIt = p.findAttribute("POSITION");
            const fastgltf::Accessor& posAccessor = gltf.accessors[positionIt->accessorIndex];
            primitiveVertexPositions.clear();
            primitiveVertexPositions.resize(posAccessor.count);
            primitiveVertexProperties.clear();
            primitiveVertexProperties.resize(posAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor, [&](fastgltf::math::fvec3 v, const size_t index) {
                VertexPosition newVertexPos{};
                newVertexPos.position = {v.x(), v.y(), v.z()};
                primitiveVertexPositions[index] = newVertexPos;

                const VertexProperty newVertexProp{};
                primitiveVertexProperties[index] = newVertexProp;
            });


            // NORMALS
            const fastgltf::Attribute* normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](fastgltf::math::fvec3 n, const size_t index) {
                    primitiveVertexProperties[index].normal = {n.x(), n.y(), n.z()};
                });
            }

            // TANGENTS
            const fastgltf::Attribute* tangents = p.findAttribute("TANGENT");
            if (tangents != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[tangents->accessorIndex], [&](fastgltf::math::fvec4 n, const size_t index) {
                    primitiveVertexProperties[index].tangent = {n.x(), n.y(), n.z(), n.w()};
                });
            }

            // UV
            const fastgltf::Attribute* uvs = p.findAttribute("TEXCOORD_0");
            if (uvs != p.attributes.end()) {
                const fastgltf::Accessor& uvAccessor = gltf.accessors[uvs->accessorIndex];

                switch (uvAccessor.componentType) {
                    case fastgltf::ComponentType::Byte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::s8vec2>(gltf, uvAccessor, [&](fastgltf::math::s8vec2 uv, const size_t index) {
                            // f = max(c / 127.0, -1.0)
                            float u = std::max(static_cast<float>(uv.x()) / 127.0f, -1.0f);
                            float v = std::max(static_cast<float>(uv.y()) / 127.0f, -1.0f);
                            primitiveVertexProperties[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::UnsignedByte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec2>(gltf, uvAccessor, [&](fastgltf::math::u8vec2 uv, const size_t index) {
                            // f = c / 255.0
                            float u = static_cast<float>(uv.x()) / 255.0f;
                            float v = static_cast<float>(uv.y()) / 255.0f;
                            primitiveVertexProperties[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::Short:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::s16vec2>(gltf, uvAccessor, [&](fastgltf::math::s16vec2 uv, const size_t index) {
                            // f = max(c / 32767.0, -1.0)
                            float u = std::max(
                                static_cast<float>(uv.x()) / 32767.0f, -1.0f);
                            float v = std::max(
                                static_cast<float>(uv.y()) / 32767.0f, -1.0f);
                            primitiveVertexProperties[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::UnsignedShort:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec2>(gltf, uvAccessor, [&](fastgltf::math::u16vec2 uv, const size_t index) {
                            // f = c / 65535.0
                            float u = static_cast<float>(uv.x()) / 65535.0f;
                            float v = static_cast<float>(uv.y()) / 65535.0f;
                            primitiveVertexProperties[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::Float:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, uvAccessor, [&](fastgltf::math::fvec2 uv, const size_t index) {
                            primitiveVertexProperties[index].uv = {uv.x(), uv.y()};
                        });
                        break;
                    default:
                        fmt::print("Unsupported UV component type: {}\n", static_cast<int>(uvAccessor.componentType));
                        break;
                }
            }

            // VERTEX COLOR
            const fastgltf::Attribute* colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex],
                                                                          [&](fastgltf::math::fvec4 color, const size_t index) {
                                                                              primitiveVertexProperties[index].color = {
                                                                                  color.x(), color.y(), color.z(), color.w()
                                                                              };
                                                                          });
            }

            primitiveData.firstIndex = static_cast<uint32_t>(model.indices.size());
            primitiveData.vertexOffset = static_cast<int32_t>(model.vertexPositions.size());
            primitiveData.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            primitiveData.boundingSphere = GenerateBoundingSphere(primitiveVertexPositions);

            model.vertexPositions.insert(model.vertexPositions.end(), primitiveVertexPositions.begin(), primitiveVertexPositions.end());
            model.vertexProperties.insert(model.vertexProperties.end(), primitiveVertexProperties.begin(), primitiveVertexProperties.end());
            model.indices.insert(model.indices.end(), primitiveIndices.begin(), primitiveIndices.end());

            meshData.primitiveIndices.push_back(model.primitives.size());
            model.primitives.push_back(primitiveData);
        }

        model.meshes.push_back(meshData);
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
    AllocatedImage newImage{};
    newImage.context = context;;

    int32_t width{};
    int32_t height{};
    int32_t nrChannels{};

    std::visit(
        fastgltf::visitor{
            [&](auto& arg) {},
            [&](const fastgltf::sources::URI& fileName) {
                assert(fileName.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(fileName.uri.isLocalPath()); // We're only capable of loading
                // local files.
                const std::wstring widePath(fileName.uri.path().begin(), fileName.uri.path().end());
                const std::filesystem::path fullPath = parentFolder / widePath;

                // if (fullPath.extension() == ".ktx") {
                //     ktxTexture1* kTexture;
                //     const ktx_error_code_e ktxResult = ktxTexture1_CreateFromNamedFile(fullPath.string().c_str(),
                //                                                                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                //                                                                        &kTexture);
                //
                //     if (ktxResult == KTX_SUCCESS) {
                //         newImage = processKtxVector(resourceManager, kTexture);
                //     }
                //
                //     ktxTexture1_Destroy(kTexture);
                // }
                // else if (fullPath.extension() == ".ktx2") {
                //     ktxTexture2* kTexture;
                //     const ktx_error_code_e ktxResult = ktxTexture2_CreateFromNamedFile(fullPath.string().c_str(),
                //                                                                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                //                                                                        &kTexture);
                //
                //     if (ktxResult == KTX_SUCCESS) {
                //         newImage = processKtxVector(resourceManager, kTexture);
                //     }
                //
                //     ktxTexture2_Destroy(kTexture);
                // }
                // else {
                unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    const size_t size = width * height * 4;
                    newImage = CreateImageFromData(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

                    stbi_image_free(data);
                }
                // }
            },
            [&](const fastgltf::sources::Array& vector) {
                if (vector.bytes.size() > 30) {
                    // Minimum size for a meaningful check
                    std::string_view strData(reinterpret_cast<const char*>(vector.bytes.data()),
                                             std::min(size_t(100), vector.bytes.size()));

                    if (strData.find("https://git-lfs.github.com/spec") != std::string_view::npos) {
                        throw std::runtime_error(
                            fmt::format("Git LFS pointer detected instead of actual texture data for image: {}. "
                                        "Please run 'git lfs pull' to retrieve the actual files.",
                                        image.name.c_str()));
                    }
                }

                //const int32_t ktxVersion = isKtxTexture(vector);
                //switch (ktxVersion) {
                // case 1:
                // {
                //     if (validateVector(vector, 0, vector.bytes.size())) {
                //         ktxTexture1* kTexture;
                //         const ktx_error_code_e ktxResult = ktxTexture1_CreateFromMemory(
                //             reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                //             vector.bytes.size(),
                //             KTX_TEXTURE_CREATE_NO_FLAGS,
                //             &kTexture);
                //
                //
                //         if (ktxResult == KTX_SUCCESS) {
                //             newImage = processKtxVector(resourceManager, kTexture);
                //         }
                //
                //         ktxTexture1_Destroy(kTexture);
                //     }
                //     else {
                //         fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                //     }
                // }
                // break;
                // case 2:
                // {
                //     if (validateVector(vector, 0, vector.bytes.size())) {
                //         ktxTexture2* kTexture;
                //
                //         const KTX_error_code ktxResult = ktxTexture2_CreateFromMemory(
                //             reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                //             vector.bytes.size(),
                //             KTX_TEXTURE_CREATE_NO_FLAGS,
                //             &kTexture);
                //
                //         if (ktxResult == KTX_SUCCESS) {
                //             newImage = processKtxVector(resourceManager, kTexture);
                //         }
                //
                //         ktxTexture2_Destroy(kTexture);
                //     }
                // }
                // break;
                // default:
                // {
                unsigned char* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                                                            static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    const size_t size = width * height * 4;
                    newImage = CreateImageFromData(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

                    stbi_image_free(data);
                }
                else {
                    fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                }
                // }
                // break;
                //}
            },
            [&](const fastgltf::sources::BufferView& view) {
                const fastgltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];
                // We only care about VectorWithMime here, because we
                // specify LoadExternalBuffers, meaning all buffers
                // are already loaded into a vector.
                std::visit(fastgltf::visitor{
                               [](auto&) {},
                               [&](const fastgltf::sources::Array& vector) {
                                   // const int32_t ktxVersion = isKtxTexture(vector);
                                   // switch (ktxVersion) {
                                   //     case 1:
                                   //     {
                                   //         if (validateVector(vector, 0, vector.bytes.size())) {
                                   //             ktxTexture1* kTexture;
                                   //             const KTX_error_code ktxResult = ktxTexture1_CreateFromMemory(
                                   //                 reinterpret_cast<const unsigned char*>(vector.bytes.data() + bufferView.byteOffset),
                                   //                 bufferView.byteLength,
                                   //                 KTX_TEXTURE_CREATE_NO_FLAGS,
                                   //                 &kTexture);
                                   //
                                   //
                                   //             if (ktxResult == KTX_SUCCESS) {
                                   //                 newImage = processKtxVector(resourceManager, kTexture);
                                   //             }
                                   //
                                   //             ktxTexture1_Destroy(kTexture);
                                   //         }
                                   //         else {
                                   //             fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                                   //         }
                                   //     }
                                   //     break;
                                   //     case 2:
                                   //     {
                                   //         if (validateVector(vector, 0, vector.bytes.size())) {
                                   //             ktxTexture2* kTexture;
                                   //
                                   //             const KTX_error_code ktxResult = ktxTexture2_CreateFromMemory(
                                   //                 reinterpret_cast<const unsigned char*>(vector.bytes.data() + bufferView.byteOffset),
                                   //                 bufferView.byteLength,
                                   //                 KTX_TEXTURE_CREATE_NO_FLAGS,
                                   //                 &kTexture);
                                   //
                                   //             if (ktxResult == KTX_SUCCESS) {
                                   //                 newImage = processKtxVector(resourceManager, kTexture);
                                   //             }
                                   //
                                   //             ktxTexture2_Destroy(kTexture);
                                   //         }
                                   //         else {
                                   //             fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                                   //         }
                                   //     }
                                   //     break;
                                   //     default:
                                   unsigned char* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
                                                                               static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;
                                       const size_t size = width * height * 4;
                                       newImage = CreateImageFromData(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
                                       stbi_image_free(data);
                                   }
                                   else {
                                       fmt::print("Error: Failed to get correct ktx version from buffer view. Fallback stbi load from memory also failed.\n");
                                   }
                                   //
                                   //         break;
                                   // }
                               }
                           }, buffer.data);
            }
        }, image.data);


    if (newImage.handle == VK_NULL_HANDLE) {
        fmt::print("Image failed to load: {}\n", image.name.c_str());
        return {};
    }

    return newImage;
}

MaterialProperties ModelLoader::ExtractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial)
{
    MaterialProperties material = {};
    material.colorFactor = glm::vec4(
        gltfMaterial.pbrData.baseColorFactor[0],
        gltfMaterial.pbrData.baseColorFactor[1],
        gltfMaterial.pbrData.baseColorFactor[2],
        gltfMaterial.pbrData.baseColorFactor[3]);

    material.metalRoughFactors.x = gltfMaterial.pbrData.metallicFactor;
    material.metalRoughFactors.y = gltfMaterial.pbrData.roughnessFactor;

    material.alphaProperties.x = gltfMaterial.alphaCutoff;
    material.alphaProperties.z = gltfMaterial.doubleSided ? 1.0f : 0.0f;
    material.alphaProperties.w = gltfMaterial.unlit ? 1.0f : 0.0f;

    switch (gltfMaterial.alphaMode) {
        case fastgltf::AlphaMode::Opaque:
            material.alphaProperties.y = static_cast<float>(MaterialType::OPAQUE_);
            break;
        case fastgltf::AlphaMode::Blend:
            material.alphaProperties.y = static_cast<float>(MaterialType::TRANSPARENT_);
            break;
        case fastgltf::AlphaMode::Mask:
            material.alphaProperties.y = static_cast<float>(MaterialType::MASK_);
            break;
    }

    material.emissiveFactor = glm::vec4(
        gltfMaterial.emissiveFactor[0],
        gltfMaterial.emissiveFactor[1],
        gltfMaterial.emissiveFactor[2],
        gltfMaterial.emissiveStrength);

    material.physicalProperties.x = gltfMaterial.ior;
    material.physicalProperties.y = gltfMaterial.dispersion;

    // Handle edge cases for missing samplers/images
    auto fixTextureIndices = [](int& imageIdx, int& samplerIdx) {
        if (imageIdx == -1 && samplerIdx != -1) imageIdx = 0;
        if (samplerIdx == -1 && imageIdx != -1) samplerIdx = 0;
    };

    if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
        LoadTextureIndicesAndUV(gltfMaterial.pbrData.baseColorTexture.value(), gltf, material.textureImageIndices.x, material.textureSamplerIndices.x, material.colorUvTransform);
        fixTextureIndices(material.textureImageIndices.x, material.textureSamplerIndices.x);
    }


    if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
        LoadTextureIndicesAndUV(gltfMaterial.pbrData.metallicRoughnessTexture.value(), gltf, material.textureImageIndices.y, material.textureSamplerIndices.y, material.metalRoughUvTransform);
        fixTextureIndices(material.textureImageIndices.y, material.textureSamplerIndices.y);
    }

    if (gltfMaterial.normalTexture.has_value()) {
        LoadTextureIndicesAndUV(gltfMaterial.normalTexture.value(), gltf, material.textureImageIndices.z, material.textureSamplerIndices.z, material.normalUvTransform);
        material.physicalProperties.z = gltfMaterial.normalTexture->scale;
        fixTextureIndices(material.textureImageIndices.z, material.textureSamplerIndices.z);
    }

    if (gltfMaterial.emissiveTexture.has_value()) {
        LoadTextureIndicesAndUV(gltfMaterial.emissiveTexture.value(), gltf, material.textureImageIndices.w, material.textureSamplerIndices.w, material.emissiveUvTransform);
        fixTextureIndices(material.textureImageIndices.w, material.textureSamplerIndices.w);
    }

    if (gltfMaterial.occlusionTexture.has_value()) {
        LoadTextureIndicesAndUV(gltfMaterial.occlusionTexture.value(), gltf, material.textureImageIndices2.x, material.textureSamplerIndices2.x, material.occlusionUvTransform);
        material.physicalProperties.w = gltfMaterial.occlusionTexture->strength;
        fixTextureIndices(material.textureImageIndices2.x, material.textureSamplerIndices2.x);
    }

    if (gltfMaterial.packedNormalMetallicRoughnessTexture.has_value()) {
        LOG_ERROR("This renderer does not support packed normal metallic roughness texture.");
        CrashHandler::TriggerManualDump("This renderer does not support packed normal metallic roughness texture.");
        exit(1);
        //fixTextureIndices(material.textureImageIndices2.y, material.textureSamplerIndices2.y);
    }

    return material;
}

void ModelLoader::LoadTextureIndicesAndUV(const fastgltf::TextureInfo& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex, glm::vec4& uvTransform)
{
    const size_t textureIndex = texture.textureIndex;

    if (gltf.textures[textureIndex].basisuImageIndex.has_value()) {
        imageIndex = gltf.textures[textureIndex].basisuImageIndex.value();
    }
    else if (gltf.textures[textureIndex].imageIndex.has_value()) {
        imageIndex = gltf.textures[textureIndex].imageIndex.value();
    }

    if (gltf.textures[textureIndex].samplerIndex.has_value()) {
        samplerIndex = gltf.textures[textureIndex].samplerIndex.value();
    }

    if (texture.transform) {
        const auto& transform = texture.transform;
        uvTransform.x = transform->uvScale[0];
        uvTransform.y = transform->uvScale[1];
        uvTransform.z = transform->uvOffset[0];
        uvTransform.w = transform->uvOffset[1];
    }
}

glm::vec4 ModelLoader::GenerateBoundingSphere(const std::vector<VertexPosition>& vertices)
{
    glm::vec3 center = {0, 0, 0};

    for (auto&& vertex : vertices) {
        center += vertex.position;
    }
    center /= static_cast<float>(vertices.size());


    float radius = glm::dot(vertices[0].position - center, vertices[0].position - center);
    for (size_t i = 1; i < vertices.size(); ++i) {
        radius = std::max(radius, glm::dot(vertices[i].position - center, vertices[i].position - center));
    }
    radius = std::nextafter(sqrtf(radius), std::numeric_limits<float>::max());

    return glm::vec4(center, radius);
}

AllocatedImage ModelLoader::CreateImageFromData(unsigned char* data, size_t size, VkExtent3D imageExtent, VkFormat format, VkImageUsageFlagBits usage, bool mipmapped)
{
    VK_CHECK(vkResetFences(context->device, 1, &uploadFence));
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));
    VkCommandBuffer cmd = commandBuffer;
    const VkCommandBufferBeginInfo cmdBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // todo: can probably cache a mega staging buffer
    AllocatedBuffer uploadBuffer = VkResources::CreateAllocatedStagingBuffer(context, size);
    memcpy(uploadBuffer.allocationInfo.pMappedData, data, size);

    VkImageCreateInfo drawImageCreateInfo = VkHelpers::ImageCreateInfo(format, imageExtent, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT); // transfer src for mipmap only
    AllocatedImage newImage = VkResources::CreateAllocatedImage(context, drawImageCreateInfo);

    VkImageMemoryBarrier2 barrier = VkHelpers::ImageMemoryBarrier(
        newImage.handle,
        VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = imageExtent;

    vkCmdCopyBufferToImage(cmd, uploadBuffer.handle, newImage.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);


    // todo: mipmapping
    // if (mipmapped) {
    //     vk_helpers::generateMipmaps(cmd, newImage->image, VkExtent2D{newImage->imageExtent.width, newImage->imageExtent.height});
    // }
    // else {
    // }
    barrier = VkHelpers::ImageMemoryBarrier(
        newImage.handle,
        VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // fragment sample
    );

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VK_CHECK(vkEndCommandBuffer(cmd));
    VkCommandBufferSubmitInfo cmdSubmitInfo = VkHelpers::CommandBufferSubmitInfo(cmd);
    const VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&cmdSubmitInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(context->graphicsQueue, 1, &submitInfo, uploadFence));
    VK_CHECK(vkWaitForFences(context->device, 1, &uploadFence, true, 1000000000));

    newImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    uploadBuffer.Release();
    return newImage;
}
} // Renderer
