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
    MeshletVerticesData* meshletVerticesBuffer;
    MeshletTrianglesData* meshletTrianglesBuffer;
    MeshletData* meshletBuffer;

    MeshIndirectParameterBuffer* meshIndirectParameterBuffer;

    // Dynamics
    MaterialData* materialBuffer; // well, this ones not yet dynamic
    ModelData* modelBuffer;
};

bool frustumCullSphere(Frustum frustum, float4 sphere) {
    // sphere.xyz = center, sphere.w = radius
    for (int i = 0; i < 6; i++) {
        float4 plane = frustum.planes[i];
        float dist = dot(plane.xyz, sphere.xyz) + plane.w;
        if (dist < -sphere.w) {
            return false;
        }
    }
    return true;
}

const static int32_t TASK_SHADER_DISPATCH_X = 64;
const static int32_t MESH_SHADER_DISPATCH_X = 32;
const static uint MAX_VERTICES = 64;
const static uint MAX_PRIMITIVES = 64;

// Per https://developer.nvidia.com/blog/using-mesh-shaders-for-professional-graphics/
// Task shader output should ideally be below 236/108. We can hit 108 if we turn uint32_t to uint8_t and store offsets in the payload.
// 4 + 4 + 4 + 64 = 86
struct MeshletPayload {
    uint32_t modelIndex;
    uint32_t materialIndex;
    uint32_t groupMeshletOffset;

    uint8_t meshletIndices[TASK_SHADER_DISPATCH_X];
};

groupshared MeshletPayload sharedPayload;
groupshared Atomic<uint32_t> visibleMeshletCount;

[shader("task")]
[numthreads(TASK_SHADER_DISPATCH_X, 1, 1)]
void taskMain(uniform PushConsts pushConstants,
    uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint drawIndex : SV_DrawIndex)
{
    MeshIndirectDrawParameters indirectParameters = pushConstants.meshIndirectParameterBuffer->parameters[drawIndex];

    if (gtid.x == 0) {
        visibleMeshletCount.store(0);

        sharedPayload.modelIndex = indirectParameters.modelIndex;
        sharedPayload.materialIndex = indirectParameters.materialIndex;
        sharedPayload.groupMeshletOffset = indirectParameters.meshletOffset + (gid.x * TASK_SHADER_DISPATCH_X);
    }

    GroupMemoryBarrierWithGroupSync();


    Model model = pushConstants.modelBuffer->models[indirectParameters.modelIndex];

    // group thread count is only 32, will never exceed uint8
    uint8_t meshletID = (uint8_t)gtid.x;
    uint localMeshletCount = min(TASK_SHADER_DISPATCH_X, indirectParameters.meshletCount - (gid.x * TASK_SHADER_DISPATCH_X));

    if (meshletID < localMeshletCount) {
        Meshlet mlet = pushConstants.meshletBuffer->meshlets[sharedPayload.groupMeshletOffset + meshletID];

        // Transform to world space
        float3 worldCenter = mul(model.modelMatrix, float4(mlet.meshletBoundingSphere.xyz, 1.0)).xyz;
        float4 worldBounds = float4(worldCenter, mlet.meshletBoundingSphere.w);

        bool visible = true;

        // Frustum cull
        if (!frustumCullSphere(pushConstants.sceneData->frustum, worldBounds)) {
            visible = false;
        }

        // Backface cull
        if (visible) {
            float3 worldConeAxis = normalize(mul((float3x3)model.modelMatrix, mlet.coneAxis));
            float3 cameraToCenter = worldCenter - pushConstants.sceneData->cameraWorldPos.xyz;
            float centerDist = length(cameraToCenter);

            // dot(center - camera, axis) >= cutoff * dist + radius
            if (dot(cameraToCenter, worldConeAxis) >= mlet.coneCutoff * centerDist + mlet.meshletBoundingSphere.w) {
                visible = false;  // Backface culled
            }
        }

        if (visible) {
            uint index = visibleMeshletCount.add(1);
            sharedPayload.meshletIndices[index] = meshletID;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (gtid.x == 0) {
        uint count = visibleMeshletCount.load();
        DispatchMesh(count, 1, 1, sharedPayload);
    }
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MESH_SHADER_DISPATCH_X, 1, 1)]
void meshMain(
    in payload MeshletPayload inPayload,
    out indices uint3 triangles[MAX_PRIMITIVES], out vertices VertexOutput vertices[MAX_VERTICES],
    uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID,
    uniform PushConsts pushConstants)
{
    uint32_t payloadMeshletIndex = gid.x;
    uint32_t meshletIndex = inPayload.groupMeshletOffset + inPayload.meshletIndices[payloadMeshletIndex];
    Meshlet meshlet = pushConstants.meshletBuffer->meshlets[meshletIndex];

    Model m = pushConstants.modelBuffer->models[inPayload.modelIndex];
    float4x4 viewProj = pushConstants.sceneData->viewProj;

    SetMeshOutputCounts(meshlet.meshletVerticesCount, meshlet.meshletTriangleCount);
    uint32_t threadIndex = gtid.x;
    for (uint i = threadIndex; i < meshlet.meshletVerticesCount; i += MESH_SHADER_DISPATCH_X) {
        uint localVertexIndex = pushConstants.meshletVerticesBuffer->meshletVertices[meshlet.meshletVerticesOffset + i].vertexIndex;
        Vertex v = pushConstants.vertexBuffer->vertices[meshlet.vertexOffset + localVertexIndex];
        float4 worldPos = mul(m.modelMatrix, float4(v.position, 1.0));
        float4 clipPos = mul(viewProj, worldPos);
        vertices[i].position = clipPos;
        vertices[i].worldPos = worldPos.xyz;
        vertices[i].normal = mul((float3x3)m.modelMatrix, v.normal);
        vertices[i].uv = v.uv;
        vertices[i].tangent = v.tangent;

        // Fancy color per-meshlet
        uint meshletID = gid.x;
        uint hash = meshletID * 747796405u + 2891336453u;
        float3 color = float3(
            (hash & 0xFF) / 255.0,
            ((hash >> 8) & 0xFF) / 255.0,
            ((hash >> 16) & 0xFF) / 255.0
        );
        vertices[i].color = float4(color, 1.0);
        // vertices[i].color = v.color;

        vertices[i].materialIndex = inPayload.materialIndex;
    }
    for (uint i = threadIndex; i < meshlet.meshletTriangleCount; i += MESH_SHADER_DISPATCH_X) {
        uint triOffset = meshlet.meshletTriangleOffset + i * 3;
        uint idx0 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 0].triangleIndex;
        uint idx1 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 1].triangleIndex;
        uint idx2 = pushConstants.meshletTrianglesBuffer->meshletTriangles[triOffset + 2].triangleIndex;
        triangles[i] = uint3(idx0, idx1, idx2);
    }
}

[shader("fragment")]
float4 fragmentMain(VertexOutput vertexInput)
{
    return float4(vertexInput.color.xyz, 1.0f);
}
