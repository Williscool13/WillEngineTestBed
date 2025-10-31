import common;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
    nointerpolation int32_t colorSamplerIndex : TEXCOORD1;
    nointerpolation int32_t colorTextureIndex : TEXCOORD2;
};

struct PushConsts {
    SceneData* sceneData;

    MaterialData* materialBuffer;
    PrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;

    ModelData* jointMatrixBuffer;
};

[[vk::binding(0, 0)]]
SamplerState samplers[];
[[vk::binding(1, 0)]]
Texture2D textures[];

[shader("vertex")]
VertexOutput vertexMain(Vertex vertex, uniform PushConsts pushConstants, uint startInstanceId : SV_StartInstanceLocation, uint vertexId : SV_VertexID) {
    VertexOutput output;

    Instance i = (*pushConstants.instanceBuffer).instances[startInstanceId];

    float4x4 skinMatrix =
        pushConstants.jointMatrixBuffer->models[vertex.joints.x + i.jointMatrixOffset].modelMatrix * vertex.weights.x +
        pushConstants.jointMatrixBuffer->models[vertex.joints.y + i.jointMatrixOffset].modelMatrix * vertex.weights.y +
        pushConstants.jointMatrixBuffer->models[vertex.joints.z + i.jointMatrixOffset].modelMatrix * vertex.weights.z +
        pushConstants.jointMatrixBuffer->models[vertex.joints.w + i.jointMatrixOffset].modelMatrix * vertex.weights.w;

    Primitive p = (*pushConstants.primitiveBuffer).primitives[i.primitiveIndex];
    MaterialProperties mt = (*pushConstants.materialBuffer).materials[p.materialIndex];

    output.position = mul(skinMatrix, float4(vertex.position, 1.0f));
    output.position = mul(pushConstants.sceneData->viewProj, output.position);

    output.uv = vertex.uv;

    output.colorSamplerIndex = mt.textureSamplerIndices.x;
    output.colorTextureIndex = mt.textureImageIndices.x;

    output.color = vertex.normal.xyz * 0.5f + 0.5f;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    if (input.colorTextureIndex >= 0 && input.colorSamplerIndex >= 0) {
        color = textures[input.colorTextureIndex].Sample(samplers[input.colorSamplerIndex], input.uv);
    }

    return float4(input.color * color.xyz, 1.0);

    // return float4(input.color, 1.0f);

    //float f = float(input.colorTextureIndex);
    //return float4(f,f,f,1.0f);
}
