import common;
import common_ui;
import common_packing;

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct PushConsts {
    SceneData* sceneData;
};

[shader("vertex")]
VertexOutput vertexMain(UIVertex vertex, uniform PushConsts pushConstants) {
    VertexOutput output;
    output.position = float4(
        (vertex.position.x / pushConstants.sceneData->renderTargetSize.x) * 2.0 - 1.0,
        (vertex.position.y / pushConstants.sceneData->renderTargetSize.y) * 2.0 - 1.0,
        0.999999,
        1.0
    );

    output.uv = vertex.uv;
    output.color = UnpackRGBA8(vertex.color);  // Unpack RGBA8 to float4

    return output;
}
[[vk::binding(0, 0)]]
Sampler2DArray combinedArrays[];

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    //return float4(1.0, 1.0, 1.0, 1.0);
    float4 color = combinedArrays[0].Sample(float3(input.uv.x, input.uv.y, 0)).x * input.color;
    return color;

}
