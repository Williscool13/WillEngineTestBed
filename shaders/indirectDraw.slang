import common;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};


struct PushConsts {
    float4x4 viewProj;

    MaterialData* materialBuffer;
    PrimitiveData* primitiveBuffer;
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
