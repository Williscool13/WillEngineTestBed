import common;

struct PushConsts {
    SceneData* sceneData;

    MeshletPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;

    MeshIndirectParameterBuffer* meshIndirectBuffer;
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

    // Each group processes TASK_SHADER_DISPATCH_X number of meshlets, so if its greater than that we need to do multiple dispatches
    uint totalMeshlets = prim.meshletCount;
    uint numChunks = (totalMeshlets + (TASK_SHADER_DISPATCH_X - 1)) / TASK_SHADER_DISPATCH_X;

    uint commandIndex = pushConstants.meshIndirectBuffer.meshIndirectCount.add(1);
    pushConstants.meshIndirectBuffer.parameters[commandIndex].groupCountX = numChunks;
    pushConstants.meshIndirectBuffer.parameters[commandIndex].groupCountY = 1;
    pushConstants.meshIndirectBuffer.parameters[commandIndex].groupCountZ = 1;
    pushConstants.meshIndirectBuffer.parameters[commandIndex].modelIndex = inst.modelIndex;
    pushConstants.meshIndirectBuffer.parameters[commandIndex].materialIndex = prim.materialIndex;
    pushConstants.meshIndirectBuffer.parameters[commandIndex].meshletOffset = prim.meshletOffset;
    pushConstants.meshIndirectBuffer.parameters[commandIndex].meshletCount = prim.meshletCount;
}
