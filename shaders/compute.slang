[[vk::binding(0, 0)]] RWTexture2D<float4> drawImage;
[[vk::binding(1, 0)]] RWTexture2D<float4> depthImage;

float3 ray_color(float3 rayDirection)
{
	float3 unitDirection = normalize(rayDirection);
	float t = 0.5*(unitDirection.y + 1.0);
	return (t)*float3(1.0, 1.0, 1.0) + (1-t)*float3(0.5, 0.7, 1.0);
}


struct PushConstants
{
    int2 renderSize;
};

[[vk::push_constant]]
PushConstants pc;

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x > pc.renderSize.x || threadId.y > pc.renderSize.y) {
        return;
    }

    uint mipLevel;
    float width;
    float height;

    int2 texelCoord = int2(threadId.xy);
    int2 size;
    drawImage.GetDimensions(size.x, size.y);

    if(texelCoord.x < size.x && texelCoord.y < size.y)
    {
        float focal_length = 1.0;
        float viewport_height = 2.0;
        float viewport_width = viewport_height * float(size.x) / float(size.y);
        float3 cam_center = float3(0, 0, 0);
        float3 viewport_u = float3(viewport_width, 0, 0);
        float3 viewport_v = float3(0, -viewport_height, 0);
        float pixel_delta_u = viewport_width / float(size.x);
        float pixel_delta_v = viewport_height / float(size.y);
        float3 upper_left = cam_center - viewport_u/2.0 + viewport_v/2.0 - float3(0, 0, focal_length);
        float3 pixel00_loc = upper_left + float3(0, 0, 0);
        float3 pixel_center = pixel00_loc + float3(float(texelCoord.x) * pixel_delta_u, float(texelCoord.y) * pixel_delta_v, 0);
        float3 ray_direction = pixel_center - cam_center;
        float4 color = float4(0.0);
        color.x = float(texelCoord.x)/(size.x);
        color.y = float(texelCoord.y)/(size.y);
        color = float4(ray_color(ray_direction), 1.0);
        drawImage[texelCoord] = color;
    };
}
