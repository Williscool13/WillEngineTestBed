import common;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float4 tangent : TEXCOORD3;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
    nointerpolation uint32_t materialIndex : TEXCOORD4;
};


struct PushConsts {
    SceneData* sceneData;

    // Statics
    VertexData* vertexBuffer;
    MeshletPrimitiveData* primitiveBuffer;
    MeshletVerticesData* meshletVerticesBuffer;
    MeshletTrianglesData* meshletTrianglesBuffer;
    MeshletData* meshletBuffer;

    // Dynamics
    MaterialData* materialBuffer; // well, this ones not yet dynamic
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;
};

const static int32_t TASK_SHADER_DISPATCH_X = 64;
const static int32_t MESH_SHADER_DISPATCH_X = 64;
const static uint MAX_VERTICES = 64;
const static uint MAX_PRIMITIVES = 64;

struct MeshletPayload
{
    uint32_t meshletOffset;
    uint32_t meshletCount;
    uint32_t modelIndex;
    uint32_t materialIndex;
};

[shader("task")]
[numthreads(TASK_SHADER_DISPATCH_X, 1, 1)]
void taskMain(uniform PushConsts pushConstants, uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Instance i = pushConstants.instanceBuffer->instances[dispatchThreadId.x];
    Model m = pushConstants.modelBuffer->models[i.modelIndex];
    MeshletPrimitive p = pushConstants.primitiveBuffer->primitives[i.primitiveIndex];

    // todo: Instance cull w/ primitive bounds

    MeshletPayload payload;
    payload.meshletOffset = p.meshletOffset;
    payload.meshletCount = p.meshletCount;
    payload.modelIndex = i.modelIndex;
    payload.materialIndex = p.materialIndex;

    int32_t xDispatch = (payload.meshletCount + (MESH_SHADER_DISPATCH_X - 1)) / MESH_SHADER_DISPATCH_X;
    DispatchMesh(xDispatch, 1, 1, payload);
}


[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MESH_SHADER_DISPATCH_X, 1, 1)]
void meshMain(
    in MeshletPayload meshletPayload,
    out indices uint3 triangles[MAX_PRIMITIVES], out vertices VertexOutput vertices[MAX_VERTICES],
    uint3 dispatchThreadId : SV_DispatchThreadID, uniform PushConsts pushConstants)
{
    if (dispatchThreadId.x >= meshletPayload.meshletCount) { return; }
    Model m = pushConstants.modelBuffer->models[meshletPayload.modelIndex];
    Meshlet meshlet = pushConstants.meshletBuffer->meshlets[meshletPayload.meshletOffset + dispatchThreadId.x];


    float4x4 viewProj = pushConstants.sceneData->viewProj;

    SetMeshOutputCounts(meshlet.meshletVerticesCount, meshlet.meshletTriangleCount);
    for (uint i = 0; i < meshlet.meshletVerticesCount; i++) {
        uint localVertexIndex = pushConstants.meshletVerticesBuffer->meshletVertices[meshlet.meshletVerticesOffset + i];
        Vertex v = pushConstants.vertexBuffer->vertices[meshlet.vertexOffset + localVertexIndex];

        float4 worldPos = mul(m.modelMatrix, float4(v.position, 1.0));
        float4 clipPos = mul(viewProj, worldPos);

        vertices[i].position = clipPos;
        vertices[i].worldPos = worldPos.xyz;
        vertices[i].normal = mul((float3x3)m.modelMatrix, v.normal);
        vertices[i].uv = v.uv;
        vertices[i].tangent = v.tangent;
        vertices[i].color = v.color.xyz;
        vertices[i].materialIndex = meshletPayload.materialIndex;
    }

    for (uint i = 0; i < meshlet.meshletTriangleCount; i++) {
        uint triOffset = meshlet.meshletTriangleOffset + i * 3;

        uint8_t idx0 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 0];
        uint8_t idx1 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 1];
        uint8_t idx2 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 2];

        triangles[i] = uint3(idx0, idx1, idx2);
    }
}

[shader("fragment")]
float4 fragmentMain(in VertexOutput vertexInput)
{
    return float4(vertexInput.color, 1.0f);
}
