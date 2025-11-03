//
// Created by William on 2025-10-20.
//

#include "model_loader.h"

#include "fastgltf/core.hpp"
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <glm/glm.hpp>

#include "model_load_utils.h"
#include "../render_utils.h"
#include "../vk_helpers.h"
#include "crash-handling/crash_handler.h"
#include "glm/gtc/quaternion.hpp"
#include "render/animation/animation_types.h"
#include "stb/stb_image.h"
#include "utils/utils.h"

namespace Renderer
{
ModelLoader::ModelLoader(VulkanContext* context)
    : context(context)
{
    VkCommandPoolCreateInfo poolInfo = VkHelpers::CommandPoolCreateInfo(context->transferQueueFamily);
    VK_CHECK(vkCreateCommandPool(context->device, &poolInfo, nullptr, &commandPool));
    VkCommandBufferAllocateInfo cmdInfo = VkHelpers::CommandBufferAllocateInfo(1, commandPool);
    VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdInfo, &commandBuffer));


    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0, // Unsignaled
    };
    VK_CHECK(vkCreateFence(context->device, &fenceInfo, nullptr, &uploadFence));

    imageStagingBuffer = VkResources::CreateAllocatedStagingBuffer(context, STAGING_SIZE);
}

ModelLoader::~ModelLoader()
{
    if (context && commandPool != VK_NULL_HANDLE) {
        // Command buffer is freed when pool is destroyed.
        vkDestroyCommandPool(context->device, commandPool, nullptr);
        vkDestroyFence(context->device, uploadFence, nullptr);
    }
}

ExtractedModel ModelLoader::LoadGltf(const std::filesystem::path& path)
{
    Utils::ScopedTimer timer{fmt::format("{} Load Time", path.filename().string())};
    ExtractedModel model{};

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

    model.bSuccessfullyLoaded = true;
    fastgltf::Asset gltf = std::move(load.get());

    model.samplers.reserve(gltf.samplers.size());
    for (const fastgltf::Sampler& gltfSampler : gltf.samplers) {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.magFilter = ModelLoadUtils::ExtractFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = ModelLoadUtils::ExtractFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));


        samplerInfo.mipmapMode = ModelLoadUtils::ExtractMipmapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Linear));

        model.samplers.push_back(VkResources::CreateSampler(context, samplerInfo));
    }

    model.images.reserve(gltf.images.size());
    // todo: job system to parallelize stb decoding.
    LoadGltfImages(gltf, path.parent_path(), model.images);

    model.imageViews.reserve(gltf.images.size());
    for (const AllocatedImage& image : model.images) {
        VkImageViewCreateInfo imageViewCreateInfo = VkHelpers::ImageViewCreateInfo(image.handle, image.format, VK_IMAGE_ASPECT_COLOR_BIT);
        model.imageViews.push_back(VkResources::CreateImageView(context, imageViewCreateInfo));
    }

    model.materials.reserve(gltf.materials.size());

    for (const fastgltf::Material& gltfMaterial : gltf.materials) {
        MaterialProperties material = ModelLoadUtils::ExtractMaterial(gltf, gltfMaterial);
        model.materials.push_back(material);
    }

    std::vector<Vertex> primitiveVertices{};
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
            primitiveVertices.clear();
            primitiveVertices.resize(posAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor, [&](fastgltf::math::fvec3 v, const size_t index) {
                primitiveVertices[index] = {};
                primitiveVertices[index].position = {v.x(), v.y(), v.z()};
            });


            // NORMALS
            const fastgltf::Attribute* normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](fastgltf::math::fvec3 n, const size_t index) {
                    primitiveVertices[index].normal = {n.x(), n.y(), n.z()};
                });
            }

            // TANGENTS
            const fastgltf::Attribute* tangents = p.findAttribute("TANGENT");
            if (tangents != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[tangents->accessorIndex], [&](fastgltf::math::fvec4 t, const size_t index) {
                    primitiveVertices[index].tangent = {t.x(), t.y(), t.z(), t.w()};
                });
            }

            // JOINTS_0
            const fastgltf::Attribute* joints0 = p.findAttribute("JOINTS_0");
            if (joints0 != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::uvec4>(gltf, gltf.accessors[joints0->accessorIndex], [&](fastgltf::math::uvec4 j, const size_t index) {
                    primitiveVertices[index].joints = {j.x(), j.y(), j.z(), j.w()};
                });
            }

            // WEIGHTS_0
            const fastgltf::Attribute* weights0 = p.findAttribute("WEIGHTS_0");
            if (weights0 != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[weights0->accessorIndex], [&](fastgltf::math::fvec4 w, const size_t index) {
                    primitiveVertices[index].weights = {w.x(), w.y(), w.z(), w.w()};
                });
            }

            primitiveData.bHasSkinning = joints0 != p.attributes.end() && weights0 != p.attributes.end();

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
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::UnsignedByte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec2>(gltf, uvAccessor, [&](fastgltf::math::u8vec2 uv, const size_t index) {
                            // f = c / 255.0
                            float u = static_cast<float>(uv.x()) / 255.0f;
                            float v = static_cast<float>(uv.y()) / 255.0f;
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::Short:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::s16vec2>(gltf, uvAccessor, [&](fastgltf::math::s16vec2 uv, const size_t index) {
                            // f = max(c / 32767.0, -1.0)
                            float u = std::max(
                                static_cast<float>(uv.x()) / 32767.0f, -1.0f);
                            float v = std::max(
                                static_cast<float>(uv.y()) / 32767.0f, -1.0f);
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::UnsignedShort:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec2>(gltf, uvAccessor, [&](fastgltf::math::u16vec2 uv, const size_t index) {
                            // f = c / 65535.0
                            float u = static_cast<float>(uv.x()) / 65535.0f;
                            float v = static_cast<float>(uv.y()) / 65535.0f;
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::Float:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, uvAccessor, [&](fastgltf::math::fvec2 uv, const size_t index) {
                            primitiveVertices[index].uv = {uv.x(), uv.y()};
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
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex], [&](const fastgltf::math::fvec4& color, const size_t index) {
                    primitiveVertices[index].color = {
                        color.x(), color.y(), color.z(), color.w()
                    };
                });
            }

            primitiveData.firstIndex = static_cast<uint32_t>(model.indices.size());
            primitiveData.vertexOffset = static_cast<int32_t>(model.vertices.size());
            primitiveData.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            primitiveData.boundingSphere = ModelLoadUtils::GenerateBoundingSphere(primitiveVertices);

            model.vertices.insert(model.vertices.end(), primitiveVertices.begin(), primitiveVertices.end());
            model.indices.insert(model.indices.end(), primitiveIndices.begin(), primitiveIndices.end());

            meshData.primitiveIndices.push_back(model.primitives.size());
            model.primitives.push_back(primitiveData);
        }

        model.meshes.push_back(meshData);
    }

    model.nodes.reserve(gltf.nodes.size());
    for (const fastgltf::Node& node : gltf.nodes) {
        Node node_{};
        node_.name = node.name;

        if (node.meshIndex.has_value()) {
            node_.meshIndex = static_cast<int>(*node.meshIndex);
        }

        std::visit(
            fastgltf::visitor{
                [&](fastgltf::math::fmat4x4 matrix) {
                    glm::mat4 glmMatrix;
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 4; ++j) {
                            glmMatrix[i][j] = matrix[i][j];
                        }
                    }

                    node_.localTranslation = glm::vec3(glmMatrix[3]);
                    node_.localRotation = glm::quat_cast(glmMatrix);
                    node_.localScale = glm::vec3(
                        glm::length(glm::vec3(glmMatrix[0])),
                        glm::length(glm::vec3(glmMatrix[1])),
                        glm::length(glm::vec3(glmMatrix[2]))
                    );
                },
                [&](fastgltf::TRS transform) {
                    node_.localTranslation = {transform.translation[0], transform.translation[1], transform.translation[2]};
                    node_.localRotation = {transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]};
                    node_.localScale = {transform.scale[0], transform.scale[1], transform.scale[2]};
                }
            }
            , node.transform
        );
        model.nodes.push_back(node_);
    }

    for (int i = 0; i < gltf.nodes.size(); i++) {
        for (std::size_t& child : gltf.nodes[i].children) {
            model.nodes[child].parent = i;
        }
    }

    // only import first skin
    if (gltf.skins.size() > 0) {
        fastgltf::Skin& skins = gltf.skins[0];

        if (gltf.skins.size() > 1) {
            LOG_WARN("Model has {} skins but only loading first skin", gltf.skins.size());
        }

        if (skins.inverseBindMatrices.has_value()) {
            const fastgltf::Accessor& inverseBindAccessor = gltf.accessors[skins.inverseBindMatrices.value()];
            model.inverseBindMatrices.resize(inverseBindAccessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(gltf, inverseBindAccessor, [&](const fastgltf::math::fmat4x4& m, const size_t index) {
                glm::mat4 glmMatrix;
                for (int col = 0; col < 4; ++col) {
                    for (int row = 0; row < 4; ++row) {
                        glmMatrix[col][row] = m[col][row];
                    }
                }
                model.inverseBindMatrices[index] = glmMatrix;
            });

            for (int32_t i = 0; i < skins.joints.size(); ++i) {
                model.nodes[skins.joints[i]].inverseBindIndex = i;
            }
        }
    }


    TopologicalSortNodes(model.nodes, model.nodeRemap);

    for (size_t i = 0; i < model.nodes.size(); ++i) {
        uint32_t depth = 0;
        uint32_t currentParent = model.nodes[i].parent;

        while (currentParent != ~0u) {
            depth++;
            currentParent = model.nodes[currentParent].parent;
        }

        model.nodes[i].depth = depth;
    }


    model.animations.reserve(gltf.animations.size());
    for (fastgltf::Animation& gltfAnim : gltf.animations) {
        Animation anim{};
        anim.name = gltfAnim.name;

        for (fastgltf::AnimationSampler& animSampler : gltfAnim.samplers) {
            AnimationSampler sampler;

            const fastgltf::Accessor& inputAccessor = gltf.accessors[animSampler.inputAccessor];
            sampler.timestamps.resize(inputAccessor.count);
            fastgltf::iterateAccessorWithIndex<float>(gltf, inputAccessor, [&](float value, size_t idx) {
                sampler.timestamps[idx] = value;
            });

            const fastgltf::Accessor& outputAccessor = gltf.accessors[animSampler.outputAccessor];
            sampler.values.resize(outputAccessor.count * fastgltf::getNumComponents(outputAccessor.type));
            if (outputAccessor.type == fastgltf::AccessorType::Vec3) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, outputAccessor,
                    [&](const fastgltf::math::fvec3& value, size_t idx) {
                        size_t baseIdx = idx * 3;  // Calculate flat base index
                        sampler.values[baseIdx + 0] = value.x();
                        sampler.values[baseIdx + 1] = value.y();
                        sampler.values[baseIdx + 2] = value.z();
                    });
            }
            else if (outputAccessor.type == fastgltf::AccessorType::Vec4) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, outputAccessor,
                    [&](const fastgltf::math::fvec4& value, size_t idx) {
                        size_t baseIdx = idx * 4;  // Calculate flat base index
                        sampler.values[baseIdx + 0] = value.x();
                        sampler.values[baseIdx + 1] = value.y();
                        sampler.values[baseIdx + 2] = value.z();
                        sampler.values[baseIdx + 3] = value.w();
                    });
            }
            else if (outputAccessor.type == fastgltf::AccessorType::Scalar) {
                fastgltf::iterateAccessorWithIndex<float>(gltf, outputAccessor,
                    [&](float value, size_t idx) {
                        sampler.values[idx] = value;  // This one is fine since it's 1 component
                    });
            }

            switch (animSampler.interpolation) {
                case fastgltf::AnimationInterpolation::Linear:
                    sampler.interpolation = AnimationSampler::Interpolation::Linear;
                    break;
                case fastgltf::AnimationInterpolation::Step:
                    sampler.interpolation = AnimationSampler::Interpolation::Step;
                    break;
                case fastgltf::AnimationInterpolation::CubicSpline:
                    sampler.interpolation = AnimationSampler::Interpolation::CubicSpline;
                    break;
            }

            anim.samplers.push_back(std::move(sampler));
        }

        anim.channels.reserve(gltfAnim.channels.size());
        for (auto& gltfChannel : gltfAnim.channels) {
            AnimationChannel channel{};
            channel.samplerIndex = gltfChannel.samplerIndex;
            channel.targetNodeIndex = gltfChannel.nodeIndex.value();

            switch (gltfChannel.path) {
                case fastgltf::AnimationPath::Translation:
                    channel.targetPath = AnimationChannel::TargetPath::Translation;
                    break;
                case fastgltf::AnimationPath::Rotation:
                    channel.targetPath = AnimationChannel::TargetPath::Rotation;
                    break;
                case fastgltf::AnimationPath::Scale:
                    channel.targetPath = AnimationChannel::TargetPath::Scale;
                    break;
                case fastgltf::AnimationPath::Weights:
                    channel.targetPath = AnimationChannel::TargetPath::Weights;
                    break;
            }

            anim.channels.push_back(channel);
        }

        anim.duration = 0.0f;
        for (const auto& sampler : anim.samplers) {
            if (!sampler.timestamps.empty()) {
                anim.duration = std::max(anim.duration, sampler.timestamps.back());
            }
        }

        model.animations.push_back(std::move(anim));
    }

    return model;
}

void ModelLoader::LoadGltfImages(const fastgltf::Asset& asset, const std::filesystem::path& parentFolder, std::vector<AllocatedImage>& outAllocatedImages)
{
    unsigned char* stbiData{nullptr};
    int32_t width{};
    int32_t height{};
    int32_t nrChannels{};

    bool bIsRecording = false;

    for (const fastgltf::Image& gltfImage : asset.images) {
        AllocatedImage newImage{};


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
                    stbiData = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);

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
                                            gltfImage.name.c_str()));
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
                    stbiData = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
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
                                       stbiData = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset), static_cast<int>(bufferView.byteLength), &width,
                                                                        &height,
                                                                        &nrChannels, 4);
                                       //
                                       //         break;
                                       // }
                                   }
                               }, buffer.data);
                }
            }, gltfImage.data);


        if (stbiData) {
            VkExtent3D imagesize;
            imagesize.width = width;
            imagesize.height = height;
            imagesize.depth = 1;
            const size_t size = width * height * 4;


            OffsetAllocator::Allocation allocation = imageStagingAllocator.allocate(size);
            if (allocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
                if (bIsRecording) {
                    // Flush staging buffer
                    VK_CHECK(vkEndCommandBuffer(commandBuffer));
                    VkCommandBufferSubmitInfo cmdSubmitInfo = VkHelpers::CommandBufferSubmitInfo(commandBuffer);
                    const VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&cmdSubmitInfo, nullptr, nullptr);
                    VK_CHECK(vkQueueSubmit2(context->transferQueue, 1, &submitInfo, uploadFence));
                    imageStagingAllocator.reset();
                    VK_CHECK(vkWaitForFences(context->device, 1, &uploadFence, true, 1000000000));
                    VK_CHECK(vkResetFences(context->device, 1, &uploadFence));
                    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));
                    bIsRecording = false;


                    // Try again
                    allocation = imageStagingAllocator.allocate(size);
                    if (allocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
                        CrashHandler::TriggerManualDump("Texture too large to fit in staging buffer. Increase staging buffer size or do not load this texture");
                        exit(1);
                    }
                }
                else {
                    // If command buffer hasn't begun, then there are no other textures queued for load - which means that this texture we're attempting to load is larger than an empty staging buffer
                    CrashHandler::TriggerManualDump("Texture too large to fit in staging buffer. Increase staging buffer size or do not load this texture");
                    exit(1);
                }
            }

            if (!bIsRecording) {
                const VkCommandBufferBeginInfo cmdBeginInfo = VkHelpers::CommandBufferBeginInfo();
                VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));
                bIsRecording = true;
            }

            newImage = RecordCreateImageFromData(commandBuffer, allocation.offset, stbiData, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

            stbi_image_free(stbiData);
            stbiData = nullptr;
        }

        outAllocatedImages.push_back(std::move(newImage));
    }

    if (bIsRecording) {
        // Final flush staging buffer
        VK_CHECK(vkEndCommandBuffer(commandBuffer));
        VkCommandBufferSubmitInfo cmdSubmitInfo = VkHelpers::CommandBufferSubmitInfo(commandBuffer);
        const VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&cmdSubmitInfo, nullptr, nullptr);
        VK_CHECK(vkQueueSubmit2(context->transferQueue, 1, &submitInfo, uploadFence));
        imageStagingAllocator.reset();
        VK_CHECK(vkWaitForFences(context->device, 1, &uploadFence, true, 1000000000));
        VK_CHECK(vkResetFences(context->device, 1, &uploadFence));
        VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));
        bIsRecording = false;
    }
}

AllocatedImage ModelLoader::RecordCreateImageFromData(VkCommandBuffer cmd, size_t offset, unsigned char* data, size_t size, VkExtent3D imageExtent, VkFormat format, VkImageUsageFlagBits usage,
                                                      bool mipmapped)
{
    char* bufferOffset = static_cast<char*>(imageStagingBuffer.allocationInfo.pMappedData) + offset;
    memcpy(bufferOffset, data, size);

    VkImageCreateInfo imageCreateInfo = VkHelpers::ImageCreateInfo(format, imageExtent, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT); // transfer src for mipmap only
    AllocatedImage newImage = VkResources::CreateAllocatedImage(context, imageCreateInfo);

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
    copyRegion.bufferOffset = offset;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = imageExtent;

    vkCmdCopyBufferToImage(cmd, imageStagingBuffer.handle, newImage.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);


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
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    // barrier.srcQueueFamilyIndex = context->transferQueueFamily;
    // barrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
    vkCmdPipelineBarrier2(cmd, &depInfo);
    newImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    return newImage;
}

void ModelLoader::TopologicalSortNodes(std::vector<Node>& nodes, std::vector<uint32_t>& oldToNew)
{
    oldToNew.resize(nodes.size());

    sortedNodes.clear();
    sortedNodes.reserve(nodes.size());

    visited.clear();
    visited.resize(nodes.size(), false);

    // Topological sort
    std::function<void(uint32_t)> visit = [&](uint32_t idx) {
        if (visited[idx]) return;
        visited[idx] = true;

        if (nodes[idx].parent != ~0u) {
            visit(nodes[idx].parent);
        }

        oldToNew[idx] = sortedNodes.size();
        sortedNodes.push_back(nodes[idx]);
    };

    for (uint32_t i = 0; i < nodes.size(); ++i) {
        visit(i);
    }

    for (auto& node : sortedNodes) {
        if (node.parent != ~0u) {
            node.parent = oldToNew[node.parent];
        }
    }

    nodes = std::move(sortedNodes);
}
} // Renderer
