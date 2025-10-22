import common;

struct PushConsts {
    float4x4 viewProj;

    PrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;
    IndexedIndirectBuffer* indirectBuffer;
    IndirectCount* indirectCount;
};

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uniform PushConsts pushConstants, uint3 threadId : SV_DispatchThreadID)
{
    uint32_t invocationId = threadId.x;
    Instance i = (*pushConstants.instanceBuffer).instances[invocationId];
    if (i.bIsAllocated == 0) { return; }
    Model m = (*pushConstants.modelBuffer).models[i.modelIndex];
    Primitive p = (*pushConstants.primitiveBuffer).primitives[i.primitiveIndex];

    VkDrawIndexedIndirectCommand cmd;
    cmd.indexCount = p.indexCount;
    cmd.firstIndex = p.firstIndex;
    cmd.vertexOffset = p.vertexOffset;
    cmd.instanceCount = 1;
    cmd.firstInstance = invocationId;

    uint outputIndex;
    InterlockedAdd((*pushConstants.indirectCount).opaqueCount, 1, outputIndex);
    pushConstants.indirectBuffer.instances[outputIndex] = cmd;
}
