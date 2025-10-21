struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float4 color;
    float2 uv;
};

struct Model
{
    float4x4 modelMatrix;
    float4x4 prevModelMatrix;
    float4 flags;
};

struct Primitive
{
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t vertexOffset;
    uint32_t bHasTransparent;
    uint32_t materialIndex;
    uint32_t padding;
    uint32_t padding1;
    uint32_t padding2;
    // {3} center, {1} radius
    float4 boundingSphere;
};


struct Instance
{
    uint32_t primitiveIndex;
    uint32_t modelIndex;
    uint32_t bIsAllocated;
    uint32_t padding;
};

struct MaterialProperties
{
    // Base PBR properties
    float4 colorFactor;
    float4 metalRoughFactors; // x: metallic, y: roughness, z: pad, w: pad

    // Texture indices
    int32_t4 textureImageIndices; // x: color, y: metallic-rough, z: normal, w: emissive
    int32_t4 textureSamplerIndices; // x: color, y: metallic-rough, z: normal, w: emissive
    int32_t4 textureImageIndices2; // x: occlusion, y: packed NRM, z: pad, w: pad
    int32_t4 textureSamplerIndices2; // x: occlusion, y: packed NRM, z: pad, w: pad

    // UV transform properties (scale.xy, offset.xy for each texture type)
    float4 colorUvTransform; // xy: scale, zw: offset
    float4 metalRoughUvTransform;
    float4 normalUvTransform;
    float4 emissiveUvTransform;
    float4 occlusionUvTransform;

    // Additional material properties
    float4 emissiveFactor; // xyz: emissive color, w: emissive strength
    float4 alphaProperties; // x: alpha cutoff, y: alpha mode, z: double sided, w: unlit
    float4 physicalProperties; // x: IOR, y: dispersion, z: normal scale, w: occlusion strength
};

struct ModelData{
    Model[] models;
}

struct InstanceData{
    Instance[] instances;
}

struct PushConsts {
    float4x4 viewProj;

    MaterialProperties *materialBuffer;
    Primitive *primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;
};


// [[vk::binding(0, 0)]]
// SamplerState samplers[];
//
// [[vk::binding(1, 0)]]
// Texture2D images[];

[shader("vertex")]
VertexOutput vertexMain(Vertex vertex, uniform PushConsts pushConstants, uint startInstanceId : SV_StartInstanceLocation, uint vertexId : SV_VertexID) {
    VertexOutput output;

    Instance i = (*pushConstants.instanceBuffer).instances[startInstanceId];
    Model m = (*pushConstants.modelBuffer).models[i.modelIndex];
    float4x4 modelMatrix = m.modelMatrix;

    //output.position = mul(mul(pushConstants.viewProj, m.modelMatrix), float4(vertex.position, 1.0f));
    //output.position = mul(float4(vertex.position, 1.0f), m.modelMatrix);
    // output.position = mul(output.position, pushConstants.viewProj);
    output.position = mul(modelMatrix, float4(vertex.position, 1.0f));
    output.position = mul(pushConstants.viewProj, output.position);
    output.color = vertex.normal * 0.5f + 0.5f;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    // float4 color = images[index].Sample(samplers[index], uv);
    return float4(input.color, 1.0);
}
