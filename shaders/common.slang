module common;

public struct Vertex
{
    public float3 position;
    public float3 normal;
    public float4 tangent;
    public float4 color;
    public float2 uv;
};

public struct Model
{
    public float4x4 modelMatrix;
    public float4x4 prevModelMatrix;
    public float4 flags;
};

public struct Primitive
{
    public uint32_t firstIndex;
    public uint32_t indexCount;
    public int32_t vertexOffset;
    public uint32_t bHasTransparent;
    public uint32_t materialIndex;
    uint32_t padding;
    uint32_t padding1;
    uint32_t padding2;
    // {3} center, {1} radius
    public float4 boundingSphere;
};


public struct Instance
{
    public uint32_t primitiveIndex;
    public uint32_t modelIndex;
    public uint32_t padding;
    public uint32_t bIsAllocated;
};

public struct MaterialProperties
{
    // Base PBR properties
    public float4 colorFactor;
    public float4 metalRoughFactors; // x: metallic, y: roughness, z: pad, w: pad

    // Texture indices
    public int32_t4 textureImageIndices; // x: color, y: metallic-rough, z: normal, w: emissive
    public int32_t4 textureSamplerIndices; // x: color, y: metallic-rough, z: normal, w: emissive
    public int32_t4 textureImageIndices2; // x: occlusion, y: packed NRM, z: pad, w: pad
    public int32_t4 textureSamplerIndices2; // x: occlusion, y: packed NRM, z: pad, w: pad

    // UV transform properties (scale.xy, offset.xy for each texture type)
    public float4 colorUvTransform; // xy: scale, zw: offset
    public float4 metalRoughUvTransform;
    public float4 normalUvTransform;
    public float4 emissiveUvTransform;
    public float4 occlusionUvTransform;

    // Additional material properties
    public float4 emissiveFactor; // xyz: emissive color, w: emissive strength
    public float4 alphaProperties; // x: alpha cutoff, y: alpha mode, z: double sided, w: unlit
    public float4 physicalProperties; // x: IOR, y: dispersion, z: normal scale, w: occlusion strength
};

public struct Scene
{
    public float4x4 view;
    public float4x4 proj;
    public float4x4 viewProj;

    public float4x4 invView;
    public float4x4 invProj;
    public float4x4 invViewProj;

    float4x4 viewProjCameraLookDirection;

    public float4x4 prevView;
    public float4x4 prevProj;
    public float4x4 prevViewProj;

    public float4x4 prevInvView;
    public float4x4 prevInvProj;
    public float4x4 prevInvViewProj;

    float4x4 prevViewProjCameraLookDirection;

    public float4 cameraWorldPos;
    public float4 prevCameraWorldPos;

    public float2 renderTargetSize;
    float2 texelSize;

    float2 cameraPlanes;

    public float deltaTime;
};

public struct SceneData {
    public Scene data;
}

public struct MaterialData {
    public MaterialProperties[] materials;
}

public struct PrimitiveData {
    public Primitive[] primitives;
}

public struct ModelData {
    public Model[] models;
}

public struct InstanceData {
    public Instance[] instances;
}

public struct VkDrawIndexedIndirectCommand
{
    public uint32_t indexCount;
    public uint32_t instanceCount;
    public uint32_t firstIndex;
    public int32_t  vertexOffset;
    public uint32_t firstInstance;
};

public struct IndirectCount
{
    public uint32_t opaqueCount;
};

public struct IndexedIndirectBuffer
{
    public VkDrawIndexedIndirectCommand[] instances;
}