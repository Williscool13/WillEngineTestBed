import common;

struct PushConsts {
    SceneData* sceneData;

    MeshletPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;

    MeshletInstanceData* indirectBuffer;
    IndirectCount* indirectCount;
};

bool isInstanceVisible(Instance inst, MeshletPrimitive prim) {
    return true;
}

const static int32_t TASK_SHADER_DISPATCH_X = 32;

[numthreads(64, 1, 1)]
void computeMain(uniform PushConsts pushConstants, uint3 dtid : SV_DispatchThreadID) {
    uint instanceID = dtid.x;

    Instance inst = pushConstants.instanceBuffer.instances[instanceID];
    if (inst.bIsAllocated == 0) { return; }

    MeshletPrimitive prim = pushConstants.primitiveBuffer.primitives[inst.primitiveIndex];

    if (!isInstanceVisible(inst, prim)) return;

    // prim.bHasTransparent;
    // prim.bHasSkinning
    // prim.boundingSphere

    uint totalMeshlets = prim.meshletCount;
    uint numChunks = (totalMeshlets + (TASK_SHADER_DISPATCH_X - 1)) / TASK_SHADER_DISPATCH_X;

    uint baseIdx = pushConstants.indirectCount->drawCount.add(numChunks);

    for (uint chunk = 0; chunk < numChunks; chunk++) {
        MeshletInstance mi;
        mi.modelIndex = inst.modelIndex;
        mi.meshletOffset = prim.meshletOffset + (chunk * 32);
        mi.meshletCount = min(32, totalMeshlets - (chunk * 32));
        mi.materialIndex = prim.materialIndex;

        pushConstants.indirectBuffer.instances[baseIdx + chunk] = mi;
    }
}
