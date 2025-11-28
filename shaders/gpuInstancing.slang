import common;
import common_instancing;

// =============== Compute Shader (Instance Visibility and Primitive Accumulation Count) ===============
struct VisibilityPushConstant {
    SceneData* sceneData;
    MeshletPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;

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

// rate - 1 instance per thread

// Out
// instance visibility buffer [uint8_t * sizeof(instanceBuffer)]
//   uint8_t bIsVisible;
//   uint16_t primitiveOffset; // later take `offset` from PrimitiveAccumulationData and write to `finalInstancedDrawBuffer[primAccData.offset + primitiveOffset]`
// primitive count buffer [uint16_t * sizeof(primitiveBuffer)] - Each primitive is limited to 65536 instances, can easily increase for scenes with over that number of a single primitive.
//   Counts number of instances
//   Atomic<uint16_t> count;

// =============== Compute Shader (Accumulation to determine offset of each instance) ===============
// In
// primitive count buffer

// rate - 1 primitive per thread

// Out
// struct PrimitiveAccumulationData {
//    uint32_t offset;
// }
// primitive accumulation buffer [PrimitiveAccumulationData * sizeof(primitiveBuffer)]


// =============== Compute Shader (Instance Draw buffer write) ===============
// In
// instance buffer
// visibility buffer
// primitive accumulation buffer

// rate - 1 instance per thread

// Out
// final instanced draw buffer [Instance * sizeof(instanceBuffer)]
//   Only contains visible and sorted by primitive instances. Worst case, all instances are visible so this must be equal to the number of instances present.
//   Practically, we can just use an indirection to point to the instance location in the instance buffer. But I feel like indirection is why min all my profiles my L1 cache hit rate is awful, so I'm copying the instance data. FYI the reason I think this is bad for L! is because of the indirection.
//  for reference: public struct Instance
// {
//     public uint32_t primitiveIndex;
//     public uint32_t modelIndex;
//     public uint32_t jointMatrixOffset;
//     public uint32_t bIsAllocated;
// };


// =============== Compute Shader (Indirect draw generation) ===============
// In
// final instanced draw buffer

// rate - 1 primitive per thread

// the draw task/mesh indirect buffer to be used to draw the whole scene
