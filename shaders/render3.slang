import common;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};


struct PushConsts {
    float4x4 modelMatrix;
    SceneData* sceneData;
};

[shader("vertex")]
VertexOutput vertexMain(Vertex vertex, uniform PushConsts pushConstants) {
    VertexOutput output;

    float4x4 modelMatrix = pushConstants.modelMatrix;

    output.position = mul(modelMatrix, float4(vertex.position, 1.0f));
    output.position = mul(pushConstants.sceneData->viewProj, output.position);

    output.color = vertex.normal * 0.5f + 0.5f;
    output.uv = vertex.uv;

    return output;
}

[[vk::binding(0, 0)]]
SamplerState samplers[];
[[vk::binding(1, 0)]]
Texture2D textures[];

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float4 color = textures[0].Sample(samplers[0], input.uv);
    return color;
}
