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
    std::array<VkCommandBuffer, ASSET_LOAD_ASYNC_COUNT> commandBuffers{};
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

    CreateDefaultResources();
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
        bool didWork = false;

        FinishUploadsInProgress();

        for (int i = static_cast<int>(modelsInProgress.size()) - 1; i >= 0; --i) {
            AssetLoadInProgress& inProgress = modelsInProgress[i];
            ModelEntry* modelEntry = models.Get(inProgress.modelEntryHandle);

            if (!modelEntry) {
                LOG_ERROR("Model handle became invalid while loading");
                modelsInProgress.erase(modelsInProgress.begin() + i);
                didWork = true;
                continue;
            }


            RemoveFinishedUploadStaging(modelEntry->uploadStagingHandles);

            if (modelEntry->uploadStagingHandles.empty()) {
                if (modelEntry->state.load() != ModelEntry::State::Ready) {
                    modelEntry->loadEndTime = std::chrono::steady_clock::now();
                    modelEntry->state.store(ModelEntry::State::Ready);

                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(modelEntry->loadEndTime - modelEntry->loadStartTime);
                    LOG_INFO("[Asset Loading Thread] Model '{}' loaded in {:.3f} ms", modelEntry->data.name, duration.count() / 1000.0);
                }

                completeQueue.push({inProgress.modelEntryHandle, std::move(inProgress.onComplete)});
                modelsInProgress.erase(modelsInProgress.begin() + i);
                didWork = true;
            }
        }

        if (uploadStagingHandleAllocator.IsAnyFree()) {
            AssetLoadRequest loadRequest;
            while (requestQueue.pop(loadRequest)) {
                ModelEntryHandle newModelHandle = LoadGltf(loadRequest.path);
                if (newModelHandle == ModelEntryHandle::Invalid) {
                    completeQueue.push({newModelHandle, std::move(loadRequest.onComplete)});
                }
                else {
                    modelsInProgress.push_back({newModelHandle, std::move(loadRequest.onComplete)});
                }
                didWork = true;
            }
        }

        if (!didWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}


void AssetLoadingThread::RequestLoad(const std::filesystem::path& path, const std::function<void(ModelEntryHandle)>& callback)
{
    requestQueue.push({path, callback});
}

void AssetLoadingThread::ResolveLoads(std::vector<ModelEntryHandle>& loadedModelsToAcquire)
{
    AssetLoadComplete loadComplete{};
    while (completeQueue.pop(loadComplete)) {
        loadedModelsToAcquire.push_back(loadComplete.handle);
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
    newModel->modelAcquires.bRequiresAcquisition = true;

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
                                                                              size_t baseIdx = idx * 3;
                                                                              sampler.values[baseIdx + 0] = value.x();
                                                                              sampler.values[baseIdx + 1] = value.y();
                                                                              sampler.values[baseIdx + 2] = value.z();
                                                                          });
            }
            else if (outputAccessor.type == fastgltf::AccessorType::Vec4) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, outputAccessor,
                                                                          [&](const fastgltf::math::fvec4& value, size_t idx) {
                                                                              size_t baseIdx = idx * 4;
                                                                              sampler.values[baseIdx + 0] = value.x();
                                                                              sampler.values[baseIdx + 1] = value.y();
                                                                              sampler.values[baseIdx + 2] = value.z();
                                                                              sampler.values[baseIdx + 3] = value.w();
                                                                          });
            }
            else if (outputAccessor.type == fastgltf::AccessorType::Scalar) {
                fastgltf::iterateAccessorWithIndex<float>(gltf, outputAccessor,
                                                          [&](float value, size_t idx) {
                                                              sampler.values[idx] = value;
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


    // Allocate all static stagings first. If fail no need to clear, because next time the handle is used, it is auto cleared
    size_t sizeVertices = vertices.size() * sizeof(Vertex);
    size_t sizeIndices = indices.size() * sizeof(uint32_t);
    size_t sizeMaterials = materials.size() * sizeof(MaterialProperties);
    size_t sizePrimitives = primitives.size() * sizeof(Primitive);

    if (sizeVertices > STAGING_BUFFER_SIZE) {
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in a single staging buffer to upload vertices of {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }
    if (sizeIndices > STAGING_BUFFER_SIZE) {
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in a single staging buffer to upload indices of {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }
    if (sizeMaterials > STAGING_BUFFER_SIZE) {
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in a single staging buffer to upload materials of {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }
    if (sizePrimitives > STAGING_BUFFER_SIZE) {
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in a single staging buffer to upload primitives of {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    newModel->data.vertexAllocation = resourceManager->vertexBufferAllocator.allocate(sizeVertices);
    if (newModel->data.vertexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in mega vertex buffer to upload {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    newModel->data.indexAllocation = resourceManager->indexBufferAllocator.allocate(sizeIndices);
    if (newModel->data.indexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        resourceManager->vertexBufferAllocator.free(newModel->data.vertexAllocation);
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in mega index buffer to upload {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    newModel->data.materialAllocation = resourceManager->materialBufferAllocator.allocate(sizeMaterials);
    if (newModel->data.materialAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        resourceManager->vertexBufferAllocator.free(newModel->data.vertexAllocation);
        resourceManager->indexBufferAllocator.free(newModel->data.indexAllocation);
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in mega material buffer to upload {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    newModel->data.primitiveAllocation = resourceManager->primitiveBufferAllocator.allocate(sizePrimitives);
    if (newModel->data.primitiveAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        resourceManager->vertexBufferAllocator.free(newModel->data.vertexAllocation);
        resourceManager->indexBufferAllocator.free(newModel->data.indexAllocation);
        resourceManager->materialBufferAllocator.free(newModel->data.materialAllocation);
        LOG_WARN("[AssetLoadingThread::LoadGltf] Not enough space in mega material buffer to upload {}", newModel->data.name);
        models.Remove(newModelHandle);
        return ModelEntryHandle::Invalid;
    }

    UploadStagingHandle uploadStagingHandle = GetAvailableStaging();
    UploadStaging* currentUploadStaging = &uploadStagingDatas[uploadStagingHandle.index];
    newModel->uploadStagingHandles.push_back(uploadStagingHandle);
    const VkCommandBufferBeginInfo cmdBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(currentUploadStaging->commandBuffer, &cmdBeginInfo));

    newModel->data.images.reserve(gltf.images.size());
    LoadGltfImages(newModel, currentUploadStaging, newModel->uploadStagingHandles, gltf, path.parent_path());

    newModel->data.imageViews.reserve(gltf.images.size());
    for (const AllocatedImage& image : newModel->data.images) {
        VkImageViewCreateInfo imageViewCreateInfo = VkHelpers::ImageViewCreateInfo(image.handle, image.format, VK_IMAGE_ASPECT_COLOR_BIT);
        newModel->data.imageViews.push_back(VkResources::CreateImageView(context, imageViewCreateInfo));
    }

    // Remap index to the real index in the descriptor
    uint32_t defaultIndex{0};
    auto remapIndices = [defaultIndex](auto& indices_, const std::vector<int32_t>& map) {
        indices_.x = indices_.x >= 0 ? map[indices_.x] : defaultIndex;
        indices_.y = indices_.y >= 0 ? map[indices_.y] : defaultIndex;
        indices_.z = indices_.z >= 0 ? map[indices_.z] : defaultIndex;
        indices_.w = indices_.w >= 0 ? map[indices_.w] : defaultIndex;
    };

    // Samplers
    newModel->data.samplerIndexToDescriptorBufferIndexMap.resize(newModel->data.samplers.size());
    for (int32_t i = 0; i < newModel->data.samplers.size(); ++i) {
        newModel->data.samplerIndexToDescriptorBufferIndexMap[i] = resourceManager->bindlessResourcesDescriptorBuffer.AllocateSampler(newModel->data.samplers[i].handle);
    }

    defaultIndex = samplerLinearDescriptorIndex;
    for (MaterialProperties& material : materials) {
        remapIndices(material.textureSamplerIndices, newModel->data.samplerIndexToDescriptorBufferIndexMap);
        remapIndices(material.textureSamplerIndices2, newModel->data.samplerIndexToDescriptorBufferIndexMap);
    }

    // Textures
    newModel->data.textureIndexToDescriptorBufferIndexMap.resize(newModel->data.imageViews.size());
    for (int32_t i = 0; i < newModel->data.imageViews.size(); ++i) {
        newModel->data.textureIndexToDescriptorBufferIndexMap[i] = resourceManager->bindlessResourcesDescriptorBuffer.AllocateTexture({
            .imageView = newModel->data.imageViews[i].handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    defaultIndex = errorImageDescriptorIndex;
    for (MaterialProperties& material : materials) {
        remapIndices(material.textureImageIndices, newModel->data.textureIndexToDescriptorBufferIndexMap);
        remapIndices(material.textureImageIndices2, newModel->data.textureIndexToDescriptorBufferIndexMap);
    }

    // Vertices
    {
        OffsetAllocator::Allocation vertexStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizeVertices);
        if (vertexStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
            StartUploadStaging(*currentUploadStaging);
            uploadStagingHandle = GetAvailableStaging();
            currentUploadStaging = &uploadStagingDatas[uploadStagingHandle.index];
            newModel->uploadStagingHandles.push_back(uploadStagingHandle);
            VK_CHECK(vkBeginCommandBuffer(currentUploadStaging->commandBuffer, &cmdBeginInfo));
            vertexStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizeIndices);
        }
        memcpy(static_cast<char*>(currentUploadStaging->stagingBuffer.allocationInfo.pMappedData) + vertexStagingAllocation.offset, vertices.data(), sizeVertices);
        VkBufferCopy2 vertexCopy{
            .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .srcOffset = vertexStagingAllocation.offset,
            .dstOffset = newModel->data.vertexAllocation.offset,
            .size = sizeVertices,
        };
        VkCopyBufferInfo2 copyVertexInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = currentUploadStaging->stagingBuffer.handle,
            .dstBuffer = resourceManager->megaVertexBuffer.handle,
            .regionCount = 1,
            .pRegions = &vertexCopy
        };
        vkCmdCopyBuffer2(currentUploadStaging->commandBuffer, &copyVertexInfo);
        VkBufferMemoryBarrier2 vertexBarrier = VkHelpers::BufferMemoryBarrier(
            resourceManager->megaVertexBuffer.handle,
            newModel->data.vertexAllocation.offset, sizeVertices,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
        vertexBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        vertexBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.dependencyFlags = 0;
        depInfo.bufferMemoryBarrierCount = 1;
        depInfo.pBufferMemoryBarriers = &vertexBarrier;
        vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);
        newModel->modelAcquires.bufferAcquireOps.push_back(VkHelpers::BufferMemoryBarrier(
                resourceManager->megaVertexBuffer.handle,
                newModel->data.vertexAllocation.offset,
                sizeVertices,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
            )
        );
        VkBufferMemoryBarrier2& acquireBarrier = newModel->modelAcquires.bufferAcquireOps.back();
        acquireBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        acquireBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
    }

    // Indices
    {
        OffsetAllocator::Allocation indexStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizeIndices);
        if (indexStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
            StartUploadStaging(*currentUploadStaging);
            uploadStagingHandle = GetAvailableStaging();
            currentUploadStaging = &uploadStagingDatas[uploadStagingHandle.index];
            newModel->uploadStagingHandles.push_back(uploadStagingHandle);
            VK_CHECK(vkBeginCommandBuffer(currentUploadStaging->commandBuffer, &cmdBeginInfo));
            indexStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizeIndices);
        }
        memcpy(static_cast<char*>(currentUploadStaging->stagingBuffer.allocationInfo.pMappedData) + indexStagingAllocation.offset, indices.data(), sizeIndices);
        VkBufferCopy2 indexCopy{
            .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .srcOffset = indexStagingAllocation.offset,
            .dstOffset = newModel->data.indexAllocation.offset,
            .size = sizeIndices,
        };
        VkCopyBufferInfo2 copyIndexInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = currentUploadStaging->stagingBuffer.handle,
            .dstBuffer = resourceManager->megaIndexBuffer.handle,
            .regionCount = 1,
            .pRegions = &indexCopy
        };
        vkCmdCopyBuffer2(currentUploadStaging->commandBuffer, &copyIndexInfo);
        VkBufferMemoryBarrier2 indexBarrier = VkHelpers::BufferMemoryBarrier(
            resourceManager->megaIndexBuffer.handle,
            newModel->data.indexAllocation.offset, sizeIndices,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
        indexBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        indexBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.dependencyFlags = 0;
        depInfo.bufferMemoryBarrierCount = 1;
        depInfo.pBufferMemoryBarriers = &indexBarrier;
        vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);
        newModel->modelAcquires.bufferAcquireOps.push_back(VkHelpers::BufferMemoryBarrier(
            resourceManager->megaIndexBuffer.handle,
            newModel->data.indexAllocation.offset,
            sizeIndices,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_INDEX_READ_BIT
        ));
        VkBufferMemoryBarrier2& acquireBarrier = newModel->modelAcquires.bufferAcquireOps.back();
        acquireBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        acquireBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
    }


    // Materials
    {
        OffsetAllocator::Allocation materialStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizeMaterials);
        if (materialStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
            StartUploadStaging(*currentUploadStaging);
            uploadStagingHandle = GetAvailableStaging();
            currentUploadStaging = &uploadStagingDatas[uploadStagingHandle.index];
            newModel->uploadStagingHandles.push_back(uploadStagingHandle);
            VK_CHECK(vkBeginCommandBuffer(currentUploadStaging->commandBuffer, &cmdBeginInfo));
            materialStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizeMaterials);
        }
        memcpy(static_cast<char*>(currentUploadStaging->stagingBuffer.allocationInfo.pMappedData) + materialStagingAllocation.offset, materials.data(), sizeMaterials);
        VkBufferCopy2 materialCopy{
            .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .srcOffset = materialStagingAllocation.offset,
            .dstOffset = newModel->data.materialAllocation.offset,
            .size = sizeMaterials,
        };
        VkCopyBufferInfo2 copyMaterialInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = currentUploadStaging->stagingBuffer.handle,
            .dstBuffer = resourceManager->materialBuffer.handle,
            .regionCount = 1,
            .pRegions = &materialCopy
        };
        vkCmdCopyBuffer2(currentUploadStaging->commandBuffer, &copyMaterialInfo);
        VkBufferMemoryBarrier2 materialBarrier = VkHelpers::BufferMemoryBarrier(
            resourceManager->materialBuffer.handle,
            newModel->data.materialAllocation.offset, sizeMaterials,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
        materialBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        materialBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.dependencyFlags = 0;
        depInfo.bufferMemoryBarrierCount = 1;
        depInfo.pBufferMemoryBarriers = &materialBarrier;
        vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);
        newModel->modelAcquires.bufferAcquireOps.push_back(VkHelpers::BufferMemoryBarrier(
            resourceManager->materialBuffer.handle,
            newModel->data.materialAllocation.offset,
            sizeMaterials,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT
        ));
        VkBufferMemoryBarrier2& acquireBarrier = newModel->modelAcquires.bufferAcquireOps.back();
        acquireBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        acquireBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
    }


    // Primitives
    uint32_t firstIndexCount = newModel->data.indexAllocation.offset / sizeof(uint32_t);
    uint32_t vertexOffsetCount = newModel->data.vertexAllocation.offset / sizeof(Vertex);
    uint32_t materialOffsetCount = newModel->data.materialAllocation.offset / sizeof(MaterialProperties);
    for (auto& primitive : primitives) {
        primitive.firstIndex += firstIndexCount;
        primitive.vertexOffset += static_cast<int32_t>(vertexOffsetCount);
        primitive.materialIndex += materialOffsetCount;
    }
    // Primitives
    {
        OffsetAllocator::Allocation primitiveStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizePrimitives);
        if (primitiveStagingAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
            StartUploadStaging(*currentUploadStaging);
            uploadStagingHandle = GetAvailableStaging();
            currentUploadStaging = &uploadStagingDatas[uploadStagingHandle.index];
            newModel->uploadStagingHandles.push_back(uploadStagingHandle);
            VK_CHECK(vkBeginCommandBuffer(currentUploadStaging->commandBuffer, &cmdBeginInfo));
            primitiveStagingAllocation = currentUploadStaging->stagingAllocator.allocate(sizePrimitives);
        }
        memcpy(static_cast<char*>(currentUploadStaging->stagingBuffer.allocationInfo.pMappedData) + primitiveStagingAllocation.offset, primitives.data(), sizePrimitives);
        VkBufferCopy2 primitiveCopy{
            .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .srcOffset = primitiveStagingAllocation.offset,
            .dstOffset = newModel->data.primitiveAllocation.offset,
            .size = sizePrimitives,
        };
        uint32_t primitiveOffsetCount = newModel->data.primitiveAllocation.offset / sizeof(Primitive);
        for (auto& mesh : newModel->data.meshes) {
            for (auto& primitiveIndex : mesh.primitiveIndices) {
                primitiveIndex += primitiveOffsetCount;
            }
        }
        VkCopyBufferInfo2 copyPrimitiveInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = currentUploadStaging->stagingBuffer.handle,
            .dstBuffer = resourceManager->primitiveBuffer.handle,
            .regionCount = 1,
            .pRegions = &primitiveCopy
        };
        vkCmdCopyBuffer2(currentUploadStaging->commandBuffer, &copyPrimitiveInfo);
        VkBufferMemoryBarrier2 primitiveBarrier = VkHelpers::BufferMemoryBarrier(
            resourceManager->primitiveBuffer.handle,
            newModel->data.primitiveAllocation.offset, sizePrimitives,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
        primitiveBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        primitiveBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.dependencyFlags = 0;
        depInfo.bufferMemoryBarrierCount = 1;
        depInfo.pBufferMemoryBarriers = &primitiveBarrier;
        vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);
        newModel->modelAcquires.bufferAcquireOps.push_back(VkHelpers::BufferMemoryBarrier(
                resourceManager->primitiveBuffer.handle,
                newModel->data.primitiveAllocation.offset,
                sizePrimitives,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            )
        );
        VkBufferMemoryBarrier2& acquireBarrier = newModel->modelAcquires.bufferAcquireOps.back();
        acquireBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
        acquireBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
    }


    StartUploadStaging(*currentUploadStaging);

    pathToHandle[path] = newModelHandle;
    return newModelHandle;
}

void AssetLoadingThread::UnloadModel(ModelEntryHandle handle)
{
    // todo: defer unload by 3 frames. It's the responsibility of the game thread to call UnloadModel responsibly
    //      (at least the same frame the resources have been removed from use in render thread)
    if (auto* entry = models.Get(handle)) {
        if (--entry->refCount == 0) {
            pathToHandle.erase(entry->data.path);
            models.Remove(handle);
        }
    }
}

ModelData* AssetLoadingThread::GetModelData(ModelEntryHandle handle)
{
    ModelEntry* modelEntry = models.Get(handle);
    if (modelEntry->state.load(std::memory_order::memory_order_acquire) != ModelEntry::State::Ready) {
        return nullptr;
    }
    return &modelEntry->data;
}

AcquireOperations* AssetLoadingThread::GetModelAcquires(ModelEntryHandle handle)
{
    ModelEntry* modelEntry = models.Get(handle);
    if (modelEntry->state.load(std::memory_order::memory_order_acquire) != ModelEntry::State::Ready) {
        return nullptr;
    }

    if (!modelEntry->modelAcquires.bRequiresAcquisition) {
        return nullptr;
    }

    return &modelEntry->modelAcquires;
}


void AssetLoadingThread::UploadTexture(const VulkanContext* context, ModelEntry* newModel, const UploadStaging* currentUploadStaging, AllocatedImage& image, const VkExtent3D extents,
                                       const uint32_t stagingOffset)
{
    VkImageMemoryBarrier2 barrier = VkHelpers::ImageMemoryBarrier(
        image.handle,
        VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = stagingOffset;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extents;

    vkCmdCopyBufferToImage(currentUploadStaging->commandBuffer, currentUploadStaging->stagingBuffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier = VkHelpers::ImageMemoryBarrier(
        image.handle,
        VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    barrier.srcQueueFamilyIndex = context->transferQueueFamily;
    barrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
    vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);


    newModel->modelAcquires.imageAcquireOps.push_back(
        VkHelpers::ImageMemoryBarrier(
            image.handle,
            VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        ));
    VkImageMemoryBarrier2& acquireBarrier = newModel->modelAcquires.imageAcquireOps.back();
    acquireBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
    acquireBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
    image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void AssetLoadingThread::CreateDefaultResources()
{
    const uint32_t white = packUnorm4x8(glm::vec4(1, 1, 1, 1));
    const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels{}; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    defaultResourcesHandle = models.Add();
    ModelEntry* newModel = models.Get(defaultResourcesHandle);
    newModel->refCount = 1;
    newModel->loadStartTime = std::chrono::steady_clock::now();
    newModel->modelAcquires.bRequiresAcquisition = true;
    newModel->data.name = "Default Resources";

    UploadStagingHandle uploadStagingHandle = GetAvailableStaging();
    UploadStaging* currentUploadStaging = &uploadStagingDatas[uploadStagingHandle.index];
    newModel->uploadStagingHandles.push_back(uploadStagingHandle);
    const VkCommandBufferBeginInfo cmdBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(currentUploadStaging->commandBuffer, &cmdBeginInfo));

    // White texture
    constexpr size_t whiteSize = 4;
    constexpr VkExtent3D whiteExtent = {1, 1, 1};
    OffsetAllocator::Allocation whiteImageAllocation = currentUploadStaging->stagingAllocator.allocate(whiteSize);
    char* whiteBufferOffset = static_cast<char*>(currentUploadStaging->stagingBuffer.allocationInfo.pMappedData) + whiteImageAllocation.offset;
    memcpy(whiteBufferOffset, &white, whiteSize);
    VkImageCreateInfo whiteImageCreateInfo = VkHelpers::ImageCreateInfo(
        VK_FORMAT_R8G8B8A8_UNORM, whiteExtent,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    whiteImage = VkResources::CreateAllocatedImage(context, whiteImageCreateInfo);
    UploadTexture(context, newModel, currentUploadStaging, whiteImage, whiteExtent, whiteImageAllocation.offset);
    VkImageViewCreateInfo whiteImageViewCreateInfo = VkHelpers::ImageViewCreateInfo(whiteImage.handle, whiteImage.format, VK_IMAGE_ASPECT_COLOR_BIT);
    whiteImageView = VkResources::CreateImageView(context, whiteImageViewCreateInfo);
    whiteImageDescriptorIndex = resourceManager->bindlessResourcesDescriptorBuffer.AllocateTexture({
        .imageView = whiteImageView.handle,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
    assert(whiteImageDescriptorIndex == 0);

    // Error texture (magenta/black checkerboard)
    constexpr size_t errorSize = sizeof(pixels);
    constexpr VkExtent3D errorExtent = {16, 16, 1};
    OffsetAllocator::Allocation errorImageAllocation = currentUploadStaging->stagingAllocator.allocate(errorSize);
    char* errorBufferOffset = static_cast<char*>(currentUploadStaging->stagingBuffer.allocationInfo.pMappedData) + errorImageAllocation.offset;
    memcpy(errorBufferOffset, pixels.data(), errorSize);
    VkImageCreateInfo errorImageCreateInfo = VkHelpers::ImageCreateInfo(
        VK_FORMAT_R8G8B8A8_UNORM, errorExtent,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    errorImage = VkResources::CreateAllocatedImage(context, errorImageCreateInfo);
    UploadTexture(context, newModel, currentUploadStaging, errorImage, errorExtent, errorImageAllocation.offset);
    VkImageViewCreateInfo errorImageViewCreateInfo = VkHelpers::ImageViewCreateInfo(errorImage.handle, errorImage.format, VK_IMAGE_ASPECT_COLOR_BIT);
    errorImageView = VkResources::CreateImageView(context, errorImageViewCreateInfo);
    errorImageDescriptorIndex = resourceManager->bindlessResourcesDescriptorBuffer.AllocateTexture({
        .imageView = errorImageView.handle,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
    assert(errorImageDescriptorIndex == 1);

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    defaultSamplerLinear = VkResources::CreateSampler(context, samplerInfo);
    samplerLinearDescriptorIndex =resourceManager->bindlessResourcesDescriptorBuffer.AllocateSampler(defaultSamplerLinear.handle);
    assert(samplerLinearDescriptorIndex == 0);

    StartUploadStaging(*currentUploadStaging);
    modelsInProgress.push_back({defaultResourcesHandle, [](ModelEntryHandle modelEntryHandle) {}});
}


void AssetLoadingThread::FinishUploadsInProgress()
{
    for (int32_t i = activeUploadHandles.size() - 1; i >= 0; --i) {
        if (!uploadStagingHandleAllocator.IsValid(activeUploadHandles[i])) {
            activeUploadHandles.erase(activeUploadHandles.begin() + i);
            continue;
        }

        VkResult fenceStatus = vkGetFenceStatus(context->device, uploadStagingDatas[activeUploadHandles[i].index].fence);
        if (fenceStatus == VK_SUCCESS) {
            uint32_t index = activeUploadHandles[i].index;
            LOG_INFO("Upload staging {} has fully executed their command buffer", index);
            uploadStagingHandleAllocator.Remove(activeUploadHandles[i]);
            activeUploadHandles.erase(activeUploadHandles.begin() + i);
        }
    }
}

void AssetLoadingThread::RemoveFinishedUploadStaging(std::vector<UploadStagingHandle>& uploadStagingHandles)
{
    if (uploadStagingHandles.empty()) { return; }

    for (size_t i = 0; i < uploadStagingHandles.size(); ++i) {
        UploadStagingHandle handle = uploadStagingHandles[i];
        if (uploadStagingHandleAllocator.IsValid(handle)) {
            return;
        }
    }

    uploadStagingHandles.clear();
}

void AssetLoadingThread::StartUploadStaging(const UploadStaging& uploadStaging)
{
    VK_CHECK(vkEndCommandBuffer(uploadStaging.commandBuffer));
    VkCommandBufferSubmitInfo cmdSubmitInfo = VkHelpers::CommandBufferSubmitInfo(uploadStaging.commandBuffer);
    const VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&cmdSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(context->transferQueue, 1, &submitInfo, uploadStaging.fence));
}

void AssetLoadingThread::LoadGltfImages(ModelEntry* newModelEntry, UploadStaging*& currentUploadStaging, std::vector<UploadStagingHandle>& uploadStagingHandles, const fastgltf::Asset& asset,
                                        const std::filesystem::path& parentFolder)
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
                    assert(fileName.fileByteOffset == 0);
                    assert(fileName.uri.isLocalPath());
                    const std::wstring widePath(fileName.uri.path().begin(), fileName.uri.path().end());
                    const std::filesystem::path fullPath = parentFolder / widePath;

                    stbiData = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);
                },
                [&](const fastgltf::sources::Array& vector) {
                    bool bPass = true;
                    if (vector.bytes.size() > 30) {
                        std::string_view strData(reinterpret_cast<const char*>(vector.bytes.data()), std::min(size_t(100), vector.bytes.size()));

                        if (strData.find("https://git-lfs.github.com/spec") != std::string_view::npos) {
                            LOG_ERROR("Git LFS pointer detected instead of actual texture data for image: {}. Please run 'git lfs pull' to retrieve the actual files.", gltfImage.name.c_str());
                            bPass = false;
                        }
                    }

                    if (bPass) {
                        stbiData = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                    }
                },
                [&](const fastgltf::sources::BufferView& view) {
                    const fastgltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                    const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];

                    std::visit(fastgltf::visitor{
                                   [](auto&) {},
                                   [&](const fastgltf::sources::Array& vector) {
                                       stbiData = stbi_load_from_memory(
                                           reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
                                           static_cast<int>(bufferView.byteLength),
                                           &width, &height, &nrChannels, 4);
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

            if (size > STAGING_BUFFER_SIZE) {
                LOG_WARN("[AssetLoadingThread::LoadGltfImages] Texture too large to fit in staging buffer: {}x{} ({}), using default error image", width, height, size);
                stbi_image_free(stbiData);
                stbiData = nullptr;
                newModelEntry->data.images.push_back(AllocatedImage{}); // VK_NULL_HANDLE
                continue;
            }

            OffsetAllocator::Allocation allocation = currentUploadStaging->stagingAllocator.allocate(size);
            if (allocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
                StartUploadStaging(*currentUploadStaging);
                UploadStagingHandle uploadStagingHandle = GetAvailableStaging();
                currentUploadStaging = &uploadStagingDatas[uploadStagingHandle.index];
                uploadStagingHandles.push_back(uploadStagingHandle);

                const VkCommandBufferBeginInfo cmdBeginInfo = VkHelpers::CommandBufferBeginInfo();
                VK_CHECK(vkBeginCommandBuffer(currentUploadStaging->commandBuffer, &cmdBeginInfo));
                allocation = currentUploadStaging->stagingAllocator.allocate(size);
            }

            char* bufferOffset = static_cast<char*>(currentUploadStaging->stagingBuffer.allocationInfo.pMappedData) + allocation.offset;
            memcpy(bufferOffset, stbiData, size);

            VkImageCreateInfo imageCreateInfo = VkHelpers::ImageCreateInfo(
                VK_FORMAT_R8G8B8A8_UNORM, imagesize,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

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
            vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = allocation.offset;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = imagesize;

            vkCmdCopyBufferToImage(currentUploadStaging->commandBuffer, currentUploadStaging->stagingBuffer.handle, newImage.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            barrier = VkHelpers::ImageMemoryBarrier(
                newImage.handle,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            barrier.srcQueueFamilyIndex = context->transferQueueFamily;
            barrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
            vkCmdPipelineBarrier2(currentUploadStaging->commandBuffer, &depInfo);
            newModelEntry->modelAcquires.imageAcquireOps.push_back(
                VkHelpers::ImageMemoryBarrier(
                    newImage.handle,
                    VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                    VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                ));
            VkImageMemoryBarrier2& acquireBarrier = newModelEntry->modelAcquires.imageAcquireOps.back();
            acquireBarrier.srcQueueFamilyIndex = context->transferQueueFamily;
            acquireBarrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
            newImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            stbi_image_free(stbiData);
            stbiData = nullptr;
        }

        newModelEntry->data.images.push_back(std::move(newImage));
    }
}

UploadStagingHandle AssetLoadingThread::GetAvailableStaging()
{
    UploadStagingHandle uploadStagingHandle = uploadStagingHandleAllocator.Add();
    if (uploadStagingHandle.IsValid()) {
        activeUploadHandles.push_back(uploadStagingHandle);
        auto& stagingUpload = uploadStagingDatas[uploadStagingHandle.index];
        stagingUpload.stagingAllocator.reset();
        VK_CHECK(vkResetFences(context->device, 1, &stagingUpload.fence));
        VK_CHECK(vkResetCommandBuffer(stagingUpload.commandBuffer, 0));

        uint32_t index = uploadStagingHandle.index;
        LOG_INFO("Upload staging {} has been retrieved", index);
        return uploadStagingHandle;
    }

    // No available in list, wait for oldest upload staging to free up
    UploadStagingHandle oldestHandle = activeUploadHandles.front();
    auto& oldestStaging = uploadStagingDatas[oldestHandle.index];
    vkWaitForFences(context->device, 1, &oldestStaging.fence, true, UINT64_MAX);

    uint32_t finishedIndex = oldestHandle.index;
    LOG_INFO("Upload staging {} has fully executed their command buffer", finishedIndex);
    uploadStagingHandleAllocator.Remove(oldestHandle);
    activeUploadHandles.erase(activeUploadHandles.begin());

    uploadStagingHandle = uploadStagingHandleAllocator.Add();
    activeUploadHandles.push_back(uploadStagingHandle);
    assert(uploadStagingHandle.IsValid());

    auto& stagingUpload = uploadStagingDatas[uploadStagingHandle.index];
    stagingUpload.stagingAllocator.reset();
    VK_CHECK(vkResetFences(context->device, 1, &stagingUpload.fence));
    VK_CHECK(vkResetCommandBuffer(stagingUpload.commandBuffer, 0));

    uint32_t index = uploadStagingHandle.index;
    LOG_INFO("Upload staging {} has been retrieved", index);
    return uploadStagingHandle;
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
