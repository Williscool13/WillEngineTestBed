import common;

struct PushConsts {
    SceneData* sceneData;

    VertexData* vertexBuffer;
    MeshletVerticesData* meshletVerticesBuffer;
    MeshletTrianglesData* meshletTrianglesBuffer;
    MeshletPrimitiveData* primitiveBuffer;

    ModelData* modelBuffer;
    InstanceData* instanceBuffer;
};

const static uint MAX_VERTICES = 64;
const static uint MAX_PRIMITIVES = 64;

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void meshMain(out indices uint3 triangles[1], out vertices VertexOutput vertices[3], uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get this meshlet
    Meshlet meshlet = pushConsts.meshletBuffer->meshlets[gid];

    // Get primitive data (contains material, transforms)
    MeshletPrimitive primitive = pushConsts.primitiveBuffer->primitives[meshlet.meshletPrimitiveIndex];

    // TODO: Get model matrix from instance/model buffers
    // For now, assume identity or you'll need to add transform index to Meshlet/MeshletPrimitive
    float4x4 modelMatrix = float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );

    float4x4 viewProj = pushConsts.sceneData->viewProj;

    // Set output counts
    SetMeshOutputCounts(meshlet.meshletVerticesCount, meshlet.meshletTriangleCount);

    // Process vertices
    for (uint i = gtid; i < meshlet.meshletVerticesCount; i += 32) {
        // Get local vertex index from meshlet vertices array
        uint localVertexIndex = pushConsts.meshletVerticesBuffer->meshletVertices[meshlet.meshletVerticesOffset + i];

        // Fetch actual vertex using meshlet's vertex offset
        Vertex v = pushConsts.vertexBuffer->vertices[meshlet.vertexOffset + localVertexIndex];

        // Transform vertex
        float4 worldPos = mul(modelMatrix, float4(v.position, 1.0));
        float4 clipPos = mul(viewProj, worldPos);

        // Output vertex attributes
        verts[i].position = clipPos;
        verts[i].worldPos = worldPos.xyz;
        verts[i].normal = mul((float3x3)modelMatrix, v.normal);
        verts[i].uv = v.uv;
        verts[i].tangent = v.tangent;
        verts[i].color = v.color;
        verts[i].materialIndex = primitive.materialIndex;
    }

    // Process triangles (unpack uint8 triplets)
    for (uint i = gtid; i < meshlet.meshletTriangleCount; i += 32) {
        uint triOffset = meshlet.meshletTriangleOffset + i * 3;

        // Unpack 3 uint8 triangle indices (local to meshlet's vertex array)
        uint idx0 = pushConsts.meshletTrianglesBuffer->meshletTriangles[triOffset + 0];
        uint idx1 = pushConsts.meshletTrianglesBuffer->meshletTriangles[triOffset + 1];
        uint idx2 = pushConsts.meshletTrianglesBuffer->meshletTriangles[triOffset + 2];

        // Output triangle with local indices
        tris[i] = uint3(idx0, idx1, idx2);
        prims[i].materialIndex = primitive.materialIndex;
    }
}
