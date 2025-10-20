//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_MODEL_H
#define WILLENGINETESTBED_MODEL_H

#include <string>

#include <volk/volk.h>

#include "render/vk_resources.h"
#include "render/vk_types.h"

namespace Renderer
{

struct Mesh
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

struct ModelData
{
    std::vector<Sampler> samplers{};
    std::vector<AllocatedImage> images{};
    std::vector<ImageView> imageViews{};

    // Split for passes that only require position (shadow pass, depth prepass)
    std::vector<MaterialProperties> materials{};
    std::vector<VertexPosition> vertexPositions{};
    std::vector<VertexProperty> vertexProperties{};
    std::vector<uint32_t> indices{};

    std::vector<Mesh> meshes{};
    std::vector<Primitive> primitives{};
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_H