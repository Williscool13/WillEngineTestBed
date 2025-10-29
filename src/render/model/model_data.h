//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_H
#define WILLENGINETESTBED_MODEL_H

#include <filesystem>
#include <string>

#include <glm/gtc/quaternion.hpp>
#include <volk/volk.h>
#include <OffsetAllocator/offsetAllocator.hpp>

#include "core/types/transform.h"
#include "render/vk_resources.h"
#include "render/vk_types.h"
#include "utils/free_list.h"

namespace Renderer
{
struct MeshInformation
{
    std::string name;
    std::vector<uint32_t> primitiveIndices;
};

struct Primitive
{
    uint32_t firstIndex{0};
    uint32_t indexCount{0};
    int32_t vertexOffset{0};
    uint32_t bHasTransparent{0};
    uint32_t materialIndex{0};
    uint32_t padding{0};
    uint32_t padding1{0};
    uint32_t padding2{0};
    // {3} center, {1} radius
    glm::vec4 boundingSphere{};
};


struct Instance
{
    uint32_t primitiveIndex{INT32_MAX};
    uint32_t modelIndex{INT32_MAX};
    uint32_t bIsAllocated{false};
    uint32_t padding{};
};

struct Model
{
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 prevModelMatrix{1.0f};
    glm::vec4 flags{1.0f}; // x: visible, y: shadow-caster, zw: reserved
};

struct Node
{
    std::string name{};
    uint32_t parent{~0u};
    uint32_t meshIndex{~0u};
    uint32_t depth{};

    glm::vec3 localTranslation{};
    glm::quat localRotation{};
    glm::vec3 localScale{};
};

struct ExtractedModel
{
    std::string name{};
    bool bSuccessfullyLoaded{false};

    std::vector<Sampler> samplers{};
    std::vector<AllocatedImage> images{};
    std::vector<ImageView> imageViews{};

    // Split for passes that only require position (shadow pass, depth prepass)
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};

    std::vector<MaterialProperties> materials{};
    std::vector<Primitive> primitives{};

    std::vector<MeshInformation> meshes{};
    std::vector<Node> nodes{};
};

struct ModelData
{
    std::string name{};
    std::filesystem::path path{};

    std::vector<MeshInformation> meshes{};
    std::vector<Node> nodes{};

    // todo: move to render/resource thread data
    std::vector<AllocatedImage> images{};
    std::vector<Sampler> samplers{};

    std::unordered_map<int32_t, int32_t> samplerIndexToDescriptorBufferIndexMap{};
    std::unordered_map<int32_t, int32_t> textureIndexToDescriptorBufferIndexMap{};

    OffsetAllocator::Allocation vertexPositionAllocation{};
    OffsetAllocator::Allocation vertexPropertyAllocation{};
    OffsetAllocator::Allocation indexAllocation{};
    OffsetAllocator::Allocation materialAllocation{};
    OffsetAllocator::Allocation primitiveAllocation{};

    ModelData() = default;

    ModelData(const ModelData&) = delete;

    ModelData& operator=(const ModelData&) = delete;

    ModelData(ModelData&&) noexcept = default;

    ModelData& operator=(ModelData&&) noexcept = default;
};

using ModelDataHandle = Handle<ModelData>;

struct ModelMatrix
{};

using ModelMatrixHandle = Handle<ModelMatrix>;

struct InstanceEntry
{};

using InstanceEntryHandle = Handle<InstanceEntry>;

struct RuntimeNode
{
    uint32_t parent{~0u};
    uint32_t depth{~0u};

    ModelDataHandle modelDataHandle{ModelDataHandle::Invalid};
    uint32_t meshIndex{~0u};

    ModelMatrixHandle modelMatrixHandle{ModelMatrixHandle::Invalid};
    std::vector<InstanceEntryHandle> instanceEntryHandles{};

    Transform transform = Transform::Identity;
    // populated when iterated upon at end of game frame
    glm::mat4 cachedWorldTransform{1.0f};

    explicit RuntimeNode(const Node& n);
};

struct RuntimeMesh
{
    // sorted when generated
    std::vector<RuntimeNode> nodes;

    Transform transform;
    bool dirty{false};
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_H
