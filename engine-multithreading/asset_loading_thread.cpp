//
// Created by William on 2025-10-20.
//

#include "asset_loading_thread.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <stb/stb_image.h>

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

#include "render/render_utils.h"
#include "render/resource_manager.h"
#include "render/vk_helpers.h"
#include "render/model/model_load_utils.h"
#include "utils/utils.h"

namespace Renderer
{
AssetLoadingThread::AssetLoadingThread() = default;

AssetLoadingThread::~AssetLoadingThread()
{
    if (context) {
        vkDestroyCommandPool(context->device, commandPool, nullptr);

        for (UploadStaging& uploadStaging : uploadStagingDatas) {
            vkDestroyFence(context->device, uploadStaging.fence, nullptr);
        }
    }
}

void AssetLoadingThread::Initialize(VulkanContext* context_, ResourceManager* resourceManager_)
{
    context = context_;
    resourceManager = resourceManager_;

    VkCommandPoolCreateInfo poolInfo = VkHelpers::CommandPoolCreateInfo(context->transferQueueFamily);
    VK_CHECK(vkCreateCommandPool(context->device, &poolInfo, nullptr, &commandPool));
    VkCommandBufferAllocateInfo cmdInfo = VkHelpers::CommandBufferAllocateInfo(4, commandPool);
    std::array<VkCommandBuffer, ASSET_LOAD_ASYNC_COUNT> commandBuffers;
    VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdInfo, commandBuffers.data()));

    for (int32_t i = 0; i < uploadStagingDatas.size(); ++i) {
        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = 0, // Unsignaled
        };
        VK_CHECK(vkCreateFence(context->device, &fenceInfo, nullptr, &uploadStagingDatas[i].fence));
        uploadStagingDatas[i].commandBuffer = commandBuffers[i];
        uploadStagingDatas[i].stagingBuffer = VkResources::CreateAllocatedStagingBuffer(context, STAGING_BUFFER_SIZE);
    }
}

void AssetLoadingThread::Start()
{
    thread = std::jthread(&AssetLoadingThread::ThreadMain, this);
}

void AssetLoadingThread::RequestShutdown()
{
    bShouldExit.store(true);
}

void AssetLoadingThread::Join()
{
    if (thread.joinable()) {
        thread.join();
    }
}

void AssetLoadingThread::ThreadMain()
{
    Utils::SetThreadName("Asset Loading Thread");

    while (!bShouldExit.load()) {
        AssetLoadRequest loadRequest;
        while (requestQueue.pop(loadRequest)) {
            ModelEntryHandle newModelHandle = LoadGltf(loadRequest.path);
            if (newModelHandle == ModelEntryHandle::Invalid) {
                completeQueue.push({newModelHandle, loadRequest.onComplete});
            }
            else {
                modelsInProgress.push_back({newModelHandle, loadRequest.onComplete});
            }
        }

        for (int i = static_cast<int>(modelsInProgress.size()) - 1; i >= 0; --i) {
            AssetLoadInProgress& inProgress = modelsInProgress[i];
            ModelEntry* modelEntry = models.Get(inProgress.modelEntryHandle);

            if (!modelEntry) {
                LOG_ERROR("Model handle became invalid while loading");
                modelsInProgress.erase(modelsInProgress.begin() + i);
                continue;
            }

            if (IsUploadFinished(modelEntry->uploadStagingHandle)) {
                if (modelEntry->state.load() != ModelEntry::State::Ready) {
                    modelEntry->loadEndTime = std::chrono::steady_clock::now();
                    modelEntry->state.store(ModelEntry::State::Ready);

                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(modelEntry->loadEndTime - modelEntry->loadStartTime);
                    LOG_INFO("[Asset Loading Thread] Model '{}' loaded in {} ms", modelEntry->data.name, duration.count());
                }

                completeQueue.push({inProgress.modelEntryHandle, inProgress.onComplete});
                modelsInProgress.erase(modelsInProgress.begin() + i);
                // todo: add acquire barrier command list to be sent from the game thread to the render thread
            }
        }
    }
}

void AssetLoadingThread::RequestLoad(const std::filesystem::path& path, const std::function<void(ModelEntryHandle)>& callback)
{
    requestQueue.push({path, callback});
}

void AssetLoadingThread::ResolveLoads()
{
    AssetLoadComplete loadComplete{};
    while (completeQueue.pop(loadComplete)) {
        loadComplete.onComplete(loadComplete.handle);
    }
}

ModelEntryHandle AssetLoadingThread::LoadGltf(const std::filesystem::path& path)
{
    if (auto it = pathToHandle.find(path); it != pathToHandle.end()) {
        models.Get(it->second)->refCount++;
        return it->second;
    }

    const ModelEntryHandle newModelHandle = models.Add();
    ModelEntry* newModel = models.Get(newModelHandle);
    newModel->refCount = 1;
    newModel->loadStartTime = std::chrono::steady_clock::now();

    fastgltf::Parser parser{fastgltf::Extensions::KHR_texture_basisu | fastgltf::Extensions::KHR_mesh_quantization | fastgltf::Extensions::KHR_texture_transform};
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
                                 | fastgltf::Options::AllowDouble
                                 | fastgltf::Options::LoadExternalBuffers
                                 | fastgltf::Options::LoadExternalImages;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!static_cast<bool>(gltfFile)) {
        LOG_ERROR("Failed to open glTF file ({}): {}\n", path.filename().string(), getErrorMessage(gltfFile.error()));
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
    if (!load) {
        LOG_ERROR("Failed to load glTF: {}\n", to_underlying(load.error()));
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    fastgltf::Asset gltf = std::move(load.get());

    //model.bSuccessfullyLoaded = true;
    newModel->data.samplers.reserve(gltf.samplers.size());
    for (const fastgltf::Sampler& gltfSampler : gltf.samplers) {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;
        samplerInfo.magFilter = ModelLoadUtils::ExtractFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = ModelLoadUtils::ExtractFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.mipmapMode = ModelLoadUtils::ExtractMipmapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Linear));
        newModel->data.samplers.push_back(VkResources::CreateSampler(context, samplerInfo));
    }

    std::vector<MaterialProperties> materials{};

    materials.reserve(gltf.materials.size());
    for (const fastgltf::Material& gltfMaterial : gltf.materials) {
        MaterialProperties material = ModelLoadUtils::ExtractMaterial(gltf, gltfMaterial);
        materials.push_back(material);
    }

    std::vector<Vertex> primitiveVertices{};
    std::vector<uint32_t> primitiveIndices{};
    std::vector<Primitive> primitives{};
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};

    newModel->data.meshes.reserve(gltf.meshes.size());
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshInformation meshData{};
        meshData.name = mesh.name;
        meshData.primitiveIndices.reserve(mesh.primitives.size());
        primitives.reserve(primitives.size() + mesh.primitives.size());

        for (fastgltf::Primitive& p : mesh.primitives) {
            Primitive primitiveData{};

            if (p.materialIndex.has_value()) {
                primitiveData.materialIndex = p.materialIndex.value();
                primitiveData.bHasTransparent = (static_cast<MaterialType>(materials[primitiveData.materialIndex].alphaProperties.y) == MaterialType::TRANSPARENT_);
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

            primitiveData.firstIndex = static_cast<uint32_t>(indices.size());
            primitiveData.vertexOffset = static_cast<int32_t>(vertices.size());
            primitiveData.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            primitiveData.boundingSphere = ModelLoadUtils::GenerateBoundingSphere(primitiveVertices);

            vertices.insert(vertices.end(), primitiveVertices.begin(), primitiveVertices.end());
            indices.insert(indices.end(), primitiveIndices.begin(), primitiveIndices.end());

            meshData.primitiveIndices.push_back(primitives.size());
            primitives.push_back(primitiveData);
        }

        newModel->data.meshes.push_back(meshData);
    }

    newModel->data.nodes.reserve(gltf.nodes.size());
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
        newModel->data.nodes.push_back(node_);
    }

    for (int i = 0; i < gltf.nodes.size(); i++) {
        for (std::size_t& child : gltf.nodes[i].children) {
            newModel->data.nodes[child].parent = i;
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
            newModel->data.inverseBindMatrices.resize(inverseBindAccessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(gltf, inverseBindAccessor, [&](const fastgltf::math::fmat4x4& m, const size_t index) {
                glm::mat4 glmMatrix;
                for (int col = 0; col < 4; ++col) {
                    for (int row = 0; row < 4; ++row) {
                        glmMatrix[col][row] = m[col][row];
                    }
                }
                newModel->data.inverseBindMatrices[index] = glmMatrix;
            });

            for (int32_t i = 0; i < skins.joints.size(); ++i) {
                newModel->data.nodes[skins.joints[i]].inverseBindIndex = i;
            }
        }
    }


    TopologicalSortNodes(newModel->data.nodes, newModel->data.nodeRemap);

    for (size_t i = 0; i < newModel->data.nodes.size(); ++i) {
        uint32_t depth = 0;
        uint32_t currentParent = newModel->data.nodes[i].parent;

        while (currentParent != ~0u) {
            depth++;
            currentParent = newModel->data.nodes[currentParent].parent;
        }

        newModel->data.nodes[i].depth = depth;
    }


    newModel->data.animations.reserve(gltf.animations.size());
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
                                                                              size_t baseIdx = idx * 3; // Calculate flat base index
                                                                              sampler.values[baseIdx + 0] = value.x();
                                                                              sampler.values[baseIdx + 1] = value.y();
                                                                              sampler.values[baseIdx + 2] = value.z();
                                                                          });
            }
            else if (outputAccessor.type == fastgltf::AccessorType::Vec4) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, outputAccessor,
                                                                          [&](const fastgltf::math::fvec4& value, size_t idx) {
                                                                              size_t baseIdx = idx * 4; // Calculate flat base index
                                                                              sampler.values[baseIdx + 0] = value.x();
                                                                              sampler.values[baseIdx + 1] = value.y();
                                                                              sampler.values[baseIdx + 2] = value.z();
                                                                              sampler.values[baseIdx + 3] = value.w();
                                                                          });
            }
            else if (outputAccessor.type == fastgltf::AccessorType::Scalar) {
                fastgltf::iterateAccessorWithIndex<float>(gltf, outputAccessor,
                                                          [&](float value, size_t idx) {
                                                              sampler.values[idx] = value; // This one is fine since it's 1 component
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

        newModel->data.animations.push_back(std::move(anim));
    }


    newModel->data.name = path.filename().string();
    newModel->data.path = path;

    UploadStaging& uploadStaging = GetAvailableTextureStaging();
    const VkCommandBufferBeginInfo cmdBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(uploadStaging.commandBuffer, &cmdBeginInfo));

    // Allocate all static stagings first. If fail no need to clear, because next time the handle is used, it is auto cleared
    size_t sizeVertices = vertices.size() * sizeof(Vertex);
    OffsetAllocator::Allocation vertexStagingAllocation = uploadStaging.stagingAllocator.allocate(sizeVertices);
    newModel->data.vertexAllocation = resourceManager->AllocateVertices(sizeVertices);
    size_t sizeIndices = indices.size() * sizeof(uint32_t);
    OffsetAllocator::Allocation indexStagingAllocation = uploadStaging.stagingAllocator.allocate(sizeIndices);
    newModel->data.indexAllocation = resourceManager->AllocateIndices(sizeIndices);
    size_t sizeMaterials = materials.size() * sizeof(MaterialProperties);
    OffsetAllocator::Allocation materialStagingAllocation = uploadStaging.stagingAllocator.allocate(sizeMaterials);
    newModel->data.materialAllocation = resourceManager->AllocateMaterials(sizeMaterials);
    size_t sizePrimitives = primitives.size() * sizeof(Primitive);
    OffsetAllocator::Allocation primitiveStagingAllocation = uploadStaging.stagingAllocator.allocate(sizePrimitives);
    newModel->data.primitiveAllocation = resourceManager->AllocatePrimitives(sizePrimitives);

    if (vertexStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE || indexStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE ||
        materialStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE || primitiveStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE ||
        newModel->data.vertexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE || newModel->data.indexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE ||
        newModel->data.materialAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE || newModel->data.primitiveAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE
    ) {
        LOG_WARN("[AssetLoadingThread::LoadGltf] LoadGltf could not fit all static data into resource/staging buffers");
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    // Images after other data, so that images don't starve the staging
    // Also so we can void vulkan resource creation if model loading is going to fail.
    newModel->data.images.reserve(gltf.images.size());
    LoadGltfImages(uploadStaging, gltf, path.parent_path(), newModel->data.images);

    newModel->data.imageViews.reserve(gltf.images.size());
    for (const AllocatedImage& image : newModel->data.images) {
        VkImageViewCreateInfo imageViewCreateInfo = VkHelpers::ImageViewCreateInfo(image.handle, image.format, VK_IMAGE_ASPECT_COLOR_BIT);
        newModel->data.imageViews.push_back(VkResources::CreateImageView(context, imageViewCreateInfo));
        // todo: if image is vk_null_handle, replace with default white image index
    }

    // Vertices
    memcpy(static_cast<char*>(uploadStaging.stagingBuffer.allocationInfo.pMappedData) + vertexStagingAllocation.offset, vertices.data(), sizeVertices);
    VkBufferCopy2 vertexCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = vertexStagingAllocation.offset,
        .dstOffset = newModel->data.vertexAllocation.offset,
        .size = sizeVertices,
    };

    // Indices
    memcpy(static_cast<char*>(uploadStaging.stagingBuffer.allocationInfo.pMappedData) + indexStagingAllocation.offset, indices.data(), sizeIndices);
    VkBufferCopy2 indexCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = indexStagingAllocation.offset,
        .dstOffset = newModel->data.indexAllocation.offset,
        .size = sizeIndices,
    };

    // Descriptor assignment can happen here. Resource upload, will need to be staged and
    auto remapIndices = [](auto& indices_, const std::vector<int32_t>& map) {
        indices_.x = indices_.x >= 0 ? map[indices_.x] : -1;
        indices_.y = indices_.y >= 0 ? map[indices_.y] : -1;
        indices_.z = indices_.z >= 0 ? map[indices_.z] : -1;
        indices_.w = indices_.w >= 0 ? map[indices_.w] : -1;
    };

    auto& bindlessResourcesDescriptorBuffer = resourceManager->GetResourceDescriptorBuffer();

    // Samplers
    newModel->data.samplerIndexToDescriptorBufferIndexMap.resize(newModel->data.samplers.size());
    for (int32_t i = 0; i < newModel->data.samplers.size(); ++i) {
        newModel->data.samplerIndexToDescriptorBufferIndexMap[i] = bindlessResourcesDescriptorBuffer.AllocateSampler(newModel->data.samplers[i].handle);
    }

    for (MaterialProperties& material : materials) {
        remapIndices(material.textureSamplerIndices, newModel->data.samplerIndexToDescriptorBufferIndexMap);
        remapIndices(material.textureSamplerIndices2, newModel->data.samplerIndexToDescriptorBufferIndexMap);
    }

    // Textures
    newModel->data.textureIndexToDescriptorBufferIndexMap.resize(newModel->data.imageViews.size());
    for (int32_t i = 0; i < newModel->data.imageViews.size(); ++i) {
        newModel->data.textureIndexToDescriptorBufferIndexMap[i] = bindlessResourcesDescriptorBuffer.AllocateTexture({
            .imageView = newModel->data.imageViews[i].handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    for (MaterialProperties& material : materials) {
        remapIndices(material.textureImageIndices, newModel->data.textureIndexToDescriptorBufferIndexMap);
        remapIndices(material.textureImageIndices2, newModel->data.textureIndexToDescriptorBufferIndexMap);
    }

    // Materials
    memcpy(static_cast<char*>(uploadStaging.stagingBuffer.allocationInfo.pMappedData) + materialStagingAllocation.offset, materials.data(), sizeMaterials);
    VkBufferCopy2 materialCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = materialStagingAllocation.offset,
        .dstOffset = newModel->data.materialAllocation.offset,
        .size = sizeMaterials,
    };

    // Primitives
    uint32_t firstIndexCount = newModel->data.indexAllocation.offset / sizeof(uint32_t);
    uint32_t vertexOffsetCount = newModel->data.vertexAllocation.offset / sizeof(Vertex);
    uint32_t materialOffsetCount = newModel->data.materialAllocation.offset / sizeof(MaterialProperties);
    for (auto& primitive : primitives) {
        primitive.firstIndex += firstIndexCount;
        primitive.vertexOffset += static_cast<int32_t>(vertexOffsetCount);
        primitive.materialIndex += materialOffsetCount;
    }

    memcpy(static_cast<char*>(uploadStaging.stagingBuffer.allocationInfo.pMappedData) + primitiveStagingAllocation.offset, primitives.data(), sizePrimitives);
    VkBufferCopy2 primitiveCopy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = primitiveStagingAllocation.offset,
        .dstOffset = newModel->data.primitiveAllocation.offset,
        .size = sizePrimitives,
    };

    uint32_t primitiveOffsetCount = primitiveStagingAllocation.offset / sizeof(Primitive);
    for (auto& mesh : newModel->data.meshes) {
        for (auto& primitiveIndex : mesh.primitiveIndices) {
            primitiveIndex += primitiveOffsetCount;
        }
    }

    VkCopyBufferInfo2 copyVertexInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = uploadStaging.stagingBuffer.handle,
        .dstBuffer = resourceManager->GetMegaVertexBuffer().handle,
        .regionCount = 1,
        .pRegions = &vertexCopy
    };
    vkCmdCopyBuffer2(uploadStaging.commandBuffer, &copyVertexInfo);
    VkCopyBufferInfo2 copyIndexInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = uploadStaging.stagingBuffer.handle,
        .dstBuffer = resourceManager->GetMegaIndexBuffer().handle,
        .regionCount = 1,
        .pRegions = &indexCopy
    };
    vkCmdCopyBuffer2(uploadStaging.commandBuffer, &copyIndexInfo);
    VkCopyBufferInfo2 copyMaterialInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = uploadStaging.stagingBuffer.handle,
        .dstBuffer = resourceManager->GetMaterialBuffer().handle,
        .regionCount = 1,
        .pRegions = &materialCopy
    };
    vkCmdCopyBuffer2(uploadStaging.commandBuffer, &copyMaterialInfo);
    VkCopyBufferInfo2 copyPrimitiveInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = uploadStaging.stagingBuffer.handle,
        .dstBuffer = resourceManager->GetPrimitiveBuffer().handle,
        .regionCount = 1,
        .pRegions = &primitiveCopy
    };
    vkCmdCopyBuffer2(uploadStaging.commandBuffer, &copyPrimitiveInfo);

    VkBufferMemoryBarrier2 barriers[4];
    barriers[0] = VkHelpers::BufferMemoryBarrier(
        resourceManager->GetMegaVertexBuffer().handle,
        newModel->data.vertexAllocation.offset, sizeVertices,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[0].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[0].dstQueueFamilyIndex = context->graphicsQueueFamily;
    barriers[1] = VkHelpers::BufferMemoryBarrier(
        resourceManager->GetMegaIndexBuffer().handle,
        newModel->data.indexAllocation.offset, sizeIndices,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[1].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[1].dstQueueFamilyIndex = context->graphicsQueueFamily;
    barriers[2] = VkHelpers::BufferMemoryBarrier(
        resourceManager->GetMaterialBuffer().handle,
        newModel->data.materialAllocation.offset, sizeMaterials,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[2].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[2].dstQueueFamilyIndex = context->graphicsQueueFamily;
    barriers[3] = VkHelpers::BufferMemoryBarrier(
        resourceManager->GetPrimitiveBuffer().handle,
        newModel->data.primitiveAllocation.offset, sizePrimitives,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    barriers[3].srcQueueFamilyIndex = context->transferQueueFamily;
    barriers[3].dstQueueFamilyIndex = context->graphicsQueueFamily;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;
    depInfo.dependencyFlags = 0;
    depInfo.bufferMemoryBarrierCount = 4;
    depInfo.pBufferMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(uploadStaging.commandBuffer, &depInfo);


    VK_CHECK(vkEndCommandBuffer(uploadStaging.commandBuffer));
    VkCommandBufferSubmitInfo cmdSubmitInfo = VkHelpers::CommandBufferSubmitInfo(uploadStaging.commandBuffer);
    const VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&cmdSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(context->transferQueue, 1, &submitInfo, uploadStaging.fence));

    newModel->uploadStagingHandle = {currentIndex, uploadStagingGenerations[currentIndex].load()};
    pathToHandle[path] = newModelHandle;
    return newModelHandle;
}

void AssetLoadingThread::UnloadModel(ModelEntryHandle handle)
{
    // todo: Send unload command to render thread
    if (auto* entry = models.Get(handle)) {
        if (--entry->refCount == 0) {
            pathToHandle.erase(entry->data.path);
            models.Remove(handle);
        }
    }
}


bool AssetLoadingThread::IsUploadFinished(UploadStagingHandle uploadStagingHandle)
{
    if (uploadStagingGenerations[uploadStagingHandle.index] > uploadStagingHandle.generation) {
        return true;
    }

    VkResult fenceStatus = vkGetFenceStatus(context->device, uploadStagingDatas[uploadStagingHandle.index].fence);
    return fenceStatus == VK_SUCCESS;
}

void AssetLoadingThread::LoadGltfImages(UploadStaging& uploadStaging, const fastgltf::Asset& asset, const std::filesystem::path& parentFolder, std::vector<AllocatedImage>& outAllocatedImages)
{
    unsigned char* stbiData{nullptr};
    int32_t width{};
    int32_t height{};
    int32_t nrChannels{};

    // todo: parallelize stbi decoding with enkiTS
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
                    bool bPass = true;
                    if (vector.bytes.size() > 30) {
                        // Minimum size for a meaningful check
                        std::string_view strData(reinterpret_cast<const char*>(vector.bytes.data()), std::min(size_t(100), vector.bytes.size()));

                        if (strData.find("https://git-lfs.github.com/spec") != std::string_view::npos) {
                            LOG_ERROR("Git LFS pointer detected instead of actual texture data for image: {}. ""Please run 'git lfs pull' to retrieve the actual files.", gltfImage.name.c_str());
                            bPass = false;
                        }
                    }

                    if (bPass) {
                        stbiData = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
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

            OffsetAllocator::Allocation allocation = uploadStaging.stagingAllocator.allocate(size);
            if (allocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
                LOG_ERROR("Texture too large to fit in staging buffer. Increase staging buffer size or do not load this texture.");
                CrashHandler::TriggerManualDump("Texture too large to fit in staging buffer. Increase staging buffer size or do not load this texture.");
                outAllocatedImages.push_back(std::move(newImage));
                continue;
            }

            char* bufferOffset = static_cast<char*>(uploadStaging.stagingBuffer.allocationInfo.pMappedData) + allocation.offset;
            memcpy(bufferOffset, stbiData, size);

            VkImageCreateInfo imageCreateInfo = VkHelpers::ImageCreateInfo(VK_FORMAT_R8G8B8A8_UNORM, imagesize,
                                                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            // transfer src for mipmap only
            newImage = VkResources::CreateAllocatedImage(context, imageCreateInfo);

            VkImageMemoryBarrier2 barrier = VkHelpers::ImageMemoryBarrier(
                newImage.handle,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
            VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(uploadStaging.commandBuffer, &depInfo);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = allocation.offset;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = imagesize;

            vkCmdCopyBufferToImage(uploadStaging.commandBuffer, uploadStaging.stagingBuffer.handle, newImage.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

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
            barrier.srcQueueFamilyIndex = context->transferQueueFamily;
            barrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
            vkCmdPipelineBarrier2(uploadStaging.commandBuffer, &depInfo);
            newImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            stbi_image_free(stbiData);
            stbiData = nullptr;
        }

        outAllocatedImages.push_back(std::move(newImage));
    }
}

UploadStaging& AssetLoadingThread::GetAvailableTextureStaging()
{
    for (uint32_t i = 0; i < ASSET_LOAD_ASYNC_COUNT; ++i) {
        uint32_t checkIndex = (currentIndex + i) % ASSET_LOAD_ASYNC_COUNT;
        auto& staging = uploadStagingDatas[checkIndex];

        if (vkGetFenceStatus(context->device, staging.fence) == VK_SUCCESS) {
            staging.stagingAllocator.reset();
            VK_CHECK(vkResetFences(context->device, 1, &staging.fence));
            VK_CHECK(vkResetCommandBuffer(staging.commandBuffer, 0));
            uploadStagingGenerations[checkIndex].fetch_add(1);
            currentIndex = checkIndex;
            return staging;
        }
    }

    // All busy, wait on next buffer
    currentIndex = (currentIndex + 1) % ASSET_LOAD_ASYNC_COUNT;
    auto& nextStaging = uploadStagingDatas[currentIndex];
    vkWaitForFences(context->device, 1, &nextStaging.fence, true, UINT64_MAX);
    nextStaging.stagingAllocator.reset();
    VK_CHECK(vkResetFences(context->device, 1, &nextStaging.fence));
    VK_CHECK(vkResetCommandBuffer(nextStaging.commandBuffer, 0));
    uploadStagingGenerations[currentIndex].fetch_add(1);
    return nextStaging;
}

void AssetLoadingThread::TopologicalSortNodes(std::vector<Node>& nodes, std::vector<uint32_t>& oldToNew)
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
}
