module common_packing;

public float4 UnpackRGBA8(uint packed) {
    return float4(
        float((packed >>  0) & 0xFF) / 255.0,
        float((packed >>  8) & 0xFF) / 255.0,
        float((packed >> 16) & 0xFF) / 255.0,
        float((packed >> 24) & 0xFF) / 255.0
    );
}