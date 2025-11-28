module common_instancing;

public struct PackedVisibility
{
    // bit-mask of visibility of 32 instances
    public uint32_t visibilityChunk;
};

public struct PackedVisibilityBuffer
{
    public PackedVisibility[] packedVisibilities;
};

// Max 65535 instance per primitive. Expand to uint32_t if needed
public struct InstancePrimitiveOffset
{
    public uint16_t primitiveOffset;
};

public struct InstancePrimitiveOffsetBuffer
{
    public InstancePrimitiveOffset[] instancePrimitiveOffsets;
};

public struct PrimitiveCount
{
    public Atomic<uint32_t> count;
    public uint32_t offset;
};

public struct PrimitiveCountBuffer
{
    public PrimitiveCount[] primitiveCounts;
};

public struct InstancedMeshIndirectDrawParameters {
    // indirect parameters
    public uint32_t groupCountX;
    public uint32_t groupCountY; // offset
    public uint32_t groupCountZ;
    public uint32_t compactedInstanceStart;

    public uint32_t meshletOffset;
    public uint32_t meshletCount;
    public uint32_t materialIndex;
    uint32_t padding;
};

public struct InstancedMeshIndirectParameterBuffer {
    public Atomic<uint32_t> meshIndirectCount;
    uint32_t padding0;
    uint32_t padding1;
    uint32_t padding2;
    public InstancedMeshIndirectDrawParameters[] parameters;
}