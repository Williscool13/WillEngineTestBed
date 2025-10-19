// No bindings

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;

    float3 positions[6] = {
        // Triangle 1 - pointing upward
        float3(cos(radians(90.0f)),  sin(radians(90.0f)), 0.0f),
        float3(cos(radians(210.0f)), sin(radians(210.0f)), 0.0f),
        float3(cos(radians(330.0f)), sin(radians(330.0f)), 0.0f),

        // Triangle 2 - pointing downward
        float3(cos(radians(30.0f)),  sin(radians(30.0f)), 0.0f),
        float3(cos(radians(150.0f)), sin(radians(150.0f)), 0.0f),
        float3(cos(radians(270.0f)), sin(radians(270.0f)), 0.0f),
    };


    uint index = vertexId % 6;
    output.position = float4(positions[index], 1.0);
    if (index < 3) {
        output.color = float3(1, 0, 0);
    } else {
        output.color = float3(0, 1, 0);
    }

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
