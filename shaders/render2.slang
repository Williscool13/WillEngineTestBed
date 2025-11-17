import common;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
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

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
