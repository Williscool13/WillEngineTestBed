module common_ui;

public struct UIVertex {
    public float2 position;
    public float2 uv;
    public uint32_t color; // packed rgba8
    public uint32_t samplerIndex;
    public uint32_t textureIndex;
    public uint32_t bIsText;
};
