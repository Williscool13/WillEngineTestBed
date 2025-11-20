import common;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float4 tangent : TEXCOORD3;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    nointerpolation uint32_t materialIndex : TEXCOORD4;
    nointerpolation uint32_t dispatchId : TEXCOORD5;
};


struct PushConsts {
    SceneData* sceneData;

    // Statics
    VertexData* vertexBuffer;
    MeshletPrimitiveData* primitiveBuffer;
    MeshletVerticesData* meshletVerticesBuffer;
    MeshletTrianglesData* meshletTrianglesBuffer;
    MeshletData* meshletBuffer;
    MeshletInstanceData* meshletInstanceBuffer;
    IndirectCount* instanceCount;


    // Dynamics
    MaterialData* materialBuffer; // well, this ones not yet dynamic
    ModelData* modelBuffer;

    uint32_t instanceIndex;
};

const static int32_t TASK_SHADER_DISPATCH_X = 1;
const static int32_t MESH_SHADER_DISPATCH_X = 32;
const static uint MAX_VERTICES = 64;
const static uint MAX_PRIMITIVES = 64;

struct MeshletPayload
{
    uint32_t meshletIndex;
    uint32_t modelIndex;
    uint32_t materialIndex;
};

groupshared MeshletPayload sharedPayloads[TASK_SHADER_DISPATCH_X];
groupshared Atomic<uint32_t> visibleMeshletCount;

bool isMeshletVisible(uint meshId) {
    return true;
}

[shader("task")]
[numthreads(TASK_SHADER_DISPATCH_X, 1, 1)]
void taskMain(uniform PushConsts pushConstants, uint3 dispatchThreadId : SV_DispatchThreadID, uint drawIndex : SV_DrawIndex)
{
    if (dispatchThreadId.x == 0) {
        visibleMeshletCount.store(0);
    }
    GroupMemoryBarrierWithGroupSync();

    uint meshletID = dispatchThreadId.x;
    MeshletInstance meshletPrimitive = pushConstants.meshletInstanceBuffer.instances[drawIndex];

    if (meshletID < meshletPrimitive.meshletCount && isMeshletVisible(meshletID)) {
        uint index = visibleMeshletCount.add(1);
        sharedPayloads[index].meshletIndex = meshletPrimitive.meshletOffset + meshletID;
        sharedPayloads[index].modelIndex = meshletPrimitive.modelIndex;
        sharedPayloads[index].materialIndex = meshletPrimitive.materialIndex;
    }

    GroupMemoryBarrierWithGroupSync();

    if (dispatchThreadId.x == 0) {
        uint count = visibleMeshletCount.load();
        DispatchMesh(count, 1, 1, sharedPayloads);
    }
}

// [shader("mesh")]
// [outputtopology("triangle")]
// [numthreads(MESH_SHADER_DISPATCH_X, 1, 1)]
// void meshMain(
//     in payload MeshletPayload sharedPayloads[TASK_SHADER_DISPATCH_X],
//     out indices uint3 triangles[MAX_PRIMITIVES], out vertices VertexOutput vertices[MAX_VERTICES],
//     uint3 groupId : SV_GroupID, uint3 gtid : SV_GroupThreadID,
//     uniform PushConsts pushConstants)
// {
//     MeshletPayload payload = sharedPayloads[gtid.y];
//     if (groupId.x >= payload.meshletCount) {
//         return;
//     }
//
//     uint meshletIdx = payload.meshletBaseIndex + groupId.x;
//     Model m = pushConstants.modelBuffer->models[payload.modelIndex];
//     Meshlet meshlet = pushConstants.meshletBuffer->meshlets[meshletIdx];
//
//     float4x4 viewProj = pushConstants.sceneData->viewProj;
//
//     SetMeshOutputCounts(meshlet.meshletVerticesCount, meshlet.meshletTriangleCount);
//
//     uint32_t instanceIndex = gtid.x;
//     for (uint i = instanceIndex; i < meshlet.meshletVerticesCount; i += MESH_SHADER_DISPATCH_X) {
//         uint localVertexIndex = pushConstants.meshletVerticesBuffer->meshletVertices[meshlet.meshletVerticesOffset + i].vertexIndex;
//         Vertex v = pushConstants.vertexBuffer->vertices[meshlet.vertexOffset + localVertexIndex];
//
//         float4 worldPos = mul(m.modelMatrix, float4(v.position, 1.0));
//         float4 clipPos = mul(viewProj, worldPos);
//
//         vertices[i].position = clipPos;
//         vertices[i].worldPos = worldPos.xyz;
//         vertices[i].normal = mul((float3x3)m.modelMatrix, v.normal);
//         vertices[i].uv = v.uv;
//         vertices[i].tangent = v.tangent;
//
//         uint meshletID = groupId.x;
//         uint hash = meshletID * 747796405u + 2891336453u;
//         float3 color = float3(
//             (hash & 0xFF) / 255.0,
//             ((hash >> 8) & 0xFF) / 255.0,
//             ((hash >> 16) & 0xFF) / 255.0
//         );
//
//         vertices[i].color = float4(color, 1.0);
//         // vertices[i].color = v.color;
//         vertices[i].materialIndex = payload.materialIndex;
//     }
//
//     for (uint i = instanceIndex; i < meshlet.meshletTriangleCount; i += MESH_SHADER_DISPATCH_X) {
//         uint triOffset = meshlet.meshletTriangleOffset + i * 3;
//
//         uint idx0 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 0].triangleIndex;
//         uint idx1 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 1].triangleIndex;
//         uint idx2 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 2].triangleIndex;
//
//         triangles[i] = uint3(idx0, idx1, idx2);
//     }
// }
//
// [shader("fragment")]
// float4 fragmentMain(VertexOutput vertexInput)
// {
//     return float4(vertexInput.color.xyz, 1.0f);
// }
