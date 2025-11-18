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

static const float4 positions[3] = {
	float4( 0.0, -1.0, 0.0, 1.0),
	float4(-1.0,  1.0, 0.0, 1.0),
	float4( 1.0,  1.0, 0.0, 1.0)
};

static const float4 colors[3] = {
	float4(0.0, 1.0, 0.0, 1.0),
	float4(0.0, 0.0, 1.0, 1.0),
	float4(1.0, 0.0, 0.0, 1.0)
};

struct DummyPayLoad
{
    uint dummyData;
};

// We don't use pay loads in this sample, but the fn call requires one
groupshared DummyPayLoad dummyPayLoad;

[shader("task")]
[numthreads(1, 1, 1)]
void taskMain()
{
    DispatchMesh(3, 1, 1, dummyPayLoad);
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void meshMain(out indices uint3 triangles[64], out vertices VertexOutput vertices[64],
        uint3 DispatchThreadID : SV_DispatchThreadID, uniform PushConsts pushConstants)
{
    float4x4 modelMatrix = pushConstants.modelMatrix;
	float4x4 mvp = mul(pushConstants.sceneData->viewProj, modelMatrix);

	float4 offset = float4(0.0, 0.0, float(DispatchThreadID.x), 0.0);

	SetMeshOutputCounts(3, 1);

	for (uint i = 0; i < 3; i++) {
		vertices[i].position = mul(mvp, positions[i] + offset);
		vertices[i].color = colors[i].xyz;
	}

	triangles[0] = uint3(0, 1, 2);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input)
{
    return float4(input.color, 1.0f);
}