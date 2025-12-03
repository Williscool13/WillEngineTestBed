import common;
import common_ui;
import common_packing;

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    nointerpolation uint textureIndex : TEXCOORD1;
    nointerpolation uint bIsText : TEXCOORD2;
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
    output.textureIndex = vertex.textureIndex;
    output.bIsText = vertex.bIsText;

    return output;
}
[[vk::binding(0, 0)]]
Sampler2DArray combinedArrays[];


[[vk::binding(0, 1)]]
SamplerState samplers[];
[[vk::binding(1, 1)]]
Texture2D textures[];


[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float4 sampled;

    if (input.bIsText == 1) {
         // Font samples atlas
        float alpha = combinedArrays[0].Sample(float3(input.uv, 0)).r;
        sampled = float4(1.0, 1.0, 1.0, alpha);
    } else {
        // image samples bindless
        // sampled = textures[NonUniformResourceIndex(input.textureIndex)].Sample(samplers[0], input.uv);
    }

    return sampled * input.color;
}


