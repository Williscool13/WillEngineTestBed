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

// Max 65535 instance per primitive.
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
};

public struct PrimitiveCountBuffer
{
    public PrimitiveCount[] primitiveCounts;
};

