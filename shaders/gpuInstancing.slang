import common;
import common_instancing;

// =============== Compute Shader (Instance Visibility and Primitive Accumulation Count) ===============
struct VisibilityPushConstant {
    // Read-Only
    SceneData* sceneData;
    MeshletPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;

    // Read-Write
    PackedVisibilityBuffer* packedVisibilityBuffer;
    InstancePrimitiveOffsetBuffer* instanceOffsetBuffer;
    PrimitiveCountBuffer* primitiveCountBuffer;
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

[shader("compute")]
[numthreads(64, 1, 1)]
void computeVisibility(uniform VisibilityPushConstant pushConstant, uint3 dtid : SV_DispatchThreadID)
{
    uint instanceID = dtid.x;

    Instance inst = pushConstant.instanceBuffer->instances[instanceID];
    if (inst.bIsAllocated == 0) { return; }

    MeshletPrimitive prim = pushConstant.primitiveBuffer->primitives[inst.primitiveIndex];
    Model model = pushConstant.modelBuffer->models[inst.modelIndex];

    float4 boundingSphere = prim.boundingSphere;
    float3 worldCenter = mul(model.modelMatrix, float4(boundingSphere.xyz, 1.0)).xyz;
    float4 worldBounds = float4(worldCenter, boundingSphere.w);

    bool bVisible = true;
    if (!frustumCullSphere(pushConstant.sceneData->frustum, worldBounds)) {
        bVisible = false;
    }

    // set visibility bit (uint32_t chunks)
    int visibilityChunkIndex = instanceID / 32;
    int visibilityChunkBit = instanceID % 32;

    if (bVisible) {
        uint32_t bitMask = 1u << visibilityChunkBit;
        InterlockedOr(pushConstant.packedVisibilityBuffer->packedVisibilities[visibilityChunkIndex].visibilityChunk, bitMask);

        uint32_t primitiveOffset = pushConstant.primitiveCountBuffer->primitiveCounts[inst.primitiveIndex].count.add(1);
        // Warning: Narrowing, if instanceCount ever intends on exceeding 65536, need to change primitiveOffset to uint32_t
        pushConstant.instanceOffsetBuffer->instancePrimitiveOffsets[instanceID].primitiveOffset = uint16_t(primitiveOffset);
    }
}

// =============== Compute Shader (Accumulation to determine offset of each instance) ===============
// Note: This is a simple single threaded prefix sum implementation. It is unlikely to be the real bottleneck of the packed instance buffer generation.
//     If needed, expand this with multi-pass complex GPU prefix sum and intermediate buffers.
struct PrefixSumPushConstant {
    // Read-Write
    PrimitiveCountBuffer* primitiveCountBuffer;

    // Read-Only
    uint32_t highestPrimitiveIndex;
};

[shader("compute")]
[numthreads(1, 1, 1)]
void computePrefixSum(uniform PrefixSumPushConstant pushConstant) {
    uint sum = 0;
    for (uint i = 0; i < pushConstant.highestPrimitiveIndex; i++) {
        pushConstant.primitiveCountBuffer->primitiveCounts[i].offset = sum;
        sum += pushConstant.primitiveCountBuffer->primitiveCounts[i].count.load();
    }
}

// =============== Compute Shader (Instance Draw buffer write) ===============
struct IndirectWritePushConstant {
    // Read-Only
    SceneData* sceneData;
    MeshletPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;

    PackedVisibilityBuffer* packedVisibilityBuffer;
    InstancePrimitiveOffsetBuffer* instanceOffsetBuffer;
    PrimitiveCountBuffer* primitiveCountBuffer;

    // Read-Write
    // todo: packed doesn't need bIsAllocated, can be optimized
    InstanceData* compactedInstanceBuffer;
    InstancedMeshIndirectParameterBuffer* indirectBuffer;
};

const static int32_t TASK_SHADER_DISPATCH_X = 64;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeCompactAndGenerateIndirect(uniform IndirectWritePushConstant pushConstant, uint3 dtid : SV_DispatchThreadID)
{
    uint instanceID = dtid.x;

    uint visChunkIndex = instanceID / 32;
    uint visChunkBit = instanceID % 32;
    uint32_t visChunk = pushConstant.packedVisibilityBuffer->packedVisibilities[visChunkIndex].visibilityChunk;
    bool visible = (visChunk & (1u << visChunkBit)) != 0;

    if (!visible) return;

    Instance inst = pushConstant.instanceBuffer->instances[instanceID];
    uint16_t primitiveOffset = pushConstant.instanceOffsetBuffer->instancePrimitiveOffsets[instanceID].primitiveOffset;
    uint32_t globalOffset = pushConstant.primitiveCountBuffer->primitiveCounts[inst.primitiveIndex].offset;

    pushConstant.compactedInstanceBuffer->instances[globalOffset + primitiveOffset] = inst;

    if (primitiveOffset == 0) {
        MeshletPrimitive prim = pushConstant.primitiveBuffer->primitives[inst.primitiveIndex];
        uint totalMeshlets = prim.meshletCount;
        uint numChunks = (totalMeshlets + (TASK_SHADER_DISPATCH_X - 1)) / TASK_SHADER_DISPATCH_X;
        uint32_t count = pushConstant.primitiveCountBuffer->primitiveCounts[inst.primitiveIndex].count.load();

        InstancedMeshIndirectDrawParameters parameters;
        parameters.groupCountX = numChunks;
        parameters.groupCountY = count;
        parameters.groupCountZ = 1;
        parameters.compactedInstanceStart = globalOffset;

        parameters.materialIndex = prim.materialIndex; // or inst.materialIndex later on in the task shader if per-instance
        parameters.meshletCount = prim.meshletCount;
        parameters.meshletOffset = prim.meshletOffset;

        uint32_t indirectIndex = pushConstant.indirectBuffer->meshIndirectCount.add(1);
        pushConstant.indirectBuffer->parameters[indirectIndex] = parameters;
    }
}
