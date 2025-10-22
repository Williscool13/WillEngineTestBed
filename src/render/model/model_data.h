//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_H
#define WILLENGINETESTBED_MODEL_H

#include <filesystem>
#include <string>

#include <volk/volk.h>
#include <OffsetAllocator/offsetAllocator.hpp>

#include "render/vk_resources.h"
#include "render/vk_types.h"

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
    uint32_t padding;
};

struct Model
{
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 prevModelMatrix{1.0f};
    glm::vec4 flags{1.0f}; // x: visible, y: shadow-caster, zw: reserved
};

struct ExtractedModel
{
    std::string name{};

    std::vector<Sampler> samplers{};
    std::vector<AllocatedImage> images{};
    std::vector<ImageView> imageViews{};

    // Split for passes that only require position (shadow pass, depth prepass)
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};

    std::vector<MaterialProperties> materials{};
    std::vector<Primitive> primitives{};

    std::vector<MeshInformation> meshes{};
};

struct ModelData
{
    std::string name{};
    std::filesystem::path path{};

    std::vector<MeshInformation> meshes{};

    OffsetAllocator::Allocation vertexPositionAllocation{};
    OffsetAllocator::Allocation vertexPropertyAllocation{};
    OffsetAllocator::Allocation indexAllocation{};
    OffsetAllocator::Allocation materialAllocation{};
    OffsetAllocator::Allocation primitiveAllocation{};
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_H
