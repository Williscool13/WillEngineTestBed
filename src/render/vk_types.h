//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_VK_TYPES_H
#define WILLENGINETESTBED_VK_TYPES_H

#include <glm/glm.hpp>
#include <volk/volk.h>

namespace Renderer
{
enum class MaterialType
{
    OPAQUE_ = 0,
    TRANSPARENT_ = 1,
    MASK_ = 2,
};

struct Vertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color{1.0f};
    glm::vec2 uv{0, 0};
    glm::uvec4 joints;
    glm::vec4 weights;
};

struct MaterialProperties
{
    // Base PBR properties
    glm::vec4 colorFactor{1.0f};
    glm::vec4 metalRoughFactors{0.0f, 1.0f, 0.0f, 0.0f}; // x: metallic, y: roughness, z: pad, w: pad

    // Texture indices
    glm::ivec4 textureImageIndices{-1}; // x: color, y: metallic-rough, z: normal, w: emissive
    glm::ivec4 textureSamplerIndices{-1}; // x: color, y: metallic-rough, z: normal, w: emissive
    glm::ivec4 textureImageIndices2{-1}; // x: occlusion, y: packed NRM, z: pad, w: pad
    glm::ivec4 textureSamplerIndices2{-1}; // x: occlusion, y: packed NRM, z: pad, w: pad

    // UV transform properties (scale.xy, offset.xy for each texture type)
    glm::vec4 colorUvTransform{1.0f, 1.0f, 0.0f, 0.0f}; // xy: scale, zw: offset
    glm::vec4 metalRoughUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 normalUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 emissiveUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 occlusionUvTransform{1.0f, 1.0f, 0.0f, 0.0f};

    // Additional material properties
    glm::vec4 emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f}; // xyz: emissive color, w: emissive strength
    glm::vec4 alphaProperties{0.5f, 0.0f, 0.0f, 0.0f}; // x: alpha cutoff, y: alpha mode, z: double sided, w: unlit
    glm::vec4 physicalProperties{1.5f, 0.0f, 1.0f, 0.0f}; // x: IOR, y: dispersion, z: normal scale, w: occlusion strength
};

struct BindlessIndirectPushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;

    VkDeviceAddress indirectBuffer;
    VkDeviceAddress indirectCountBuffer;

    VkDeviceAddress skeletalIndirectBuffer;
    VkDeviceAddress skeletalIndirectCountBuffer;
};

struct BindlessAddressPushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress materialBuffer;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;
};

struct BindlessAddressSkeletalPushConstant
{
    VkDeviceAddress sceneData;

    VkDeviceAddress materialBuffer;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;

    VkDeviceAddress jointMatrixBuffer;
};

struct IndirectCount
{
    uint32_t opaqueCount;
};

struct SceneData
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewProj{1.0f};

    glm::mat4 invView{1.0f};
    glm::mat4 invProj{1.0f};
    glm::mat4 invViewProj{1.0f};

    glm::mat4 viewProjCameraLookDirection{1.0f};

    glm::mat4 prevView{1.0f};
    glm::mat4 prevProj{1.0f};
    glm::mat4 prevViewProj{1.0f};

    glm::mat4 prevInvView{1.0f};
    glm::mat4 prevInvProj{1.0f};
    glm::mat4 prevInvViewProj{1.0f};

    glm::mat4 prevViewProjCameraLookDirection{1.0f};

    glm::vec4 cameraWorldPos{0.0f};
    glm::vec4 prevCameraWorldPos{0.0f};

    glm::vec2 renderTargetSize{};
    glm::vec2 texelSize{};

    glm::vec2 cameraPlanes{1000.0f, 0.1f};

    float deltaTime{};
};
}


#endif //WILLENGINETESTBED_VK_TYPES_H
