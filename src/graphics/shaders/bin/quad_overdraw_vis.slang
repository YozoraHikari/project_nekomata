import "../globallayout";

struct PushConstants {
    ShHandle<RWTexture2D<uint>> quadOverdrawImage;
};

[vk::push_constant] PushConstants s_pushConstants;

struct VSOut {
    float4 position : SV_Position;
}

[shader("vertex")]
VSOut main(uint vertexId: SV_VulkanVertexID) {
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float2 pos = uv * 2.0f - 1.0f;

    VSOut output;
    output.position = float4(pos, 0.0f, 1.0f);
    return output;
}
float3 PlasmaPalette(uint value, uint maxValue)
{
    float t = saturate((float)value / (float)maxValue);

    const float3 blue   = float3(0.05, 0.00, 0.55);
    const float3 purple = float3(0.55, 0.00, 0.75);
    const float3 orange = float3(1.00, 0.45, 0.00);
    const float3 yellow = float3(1.00, 0.95, 0.00);
    const float3 white  = float3(1.00, 1.00, 1.00);

    if (t < 0.25)
    {
        return lerp(blue, purple, t * 4.0);
    }
    else if (t < 0.50)
    {
        return lerp(purple, orange, (t - 0.25) * 4.0);
    }
    else if (t < 0.75)
    {
        return lerp(orange, yellow, (t - 0.50) * 4.0);
    }
    else
    {
        return lerp(yellow, white, (t - 0.75) * 4.0);
    }
}
[shader("fragment")]
float4 main(VSOut input) : SV_Target {
    float2 pixelpos = input.position.xy - 0.5f;
    uint2 quadpos = uint2(floor(pixelpos / 2.0f));
    uint overdraw = s_pushConstants.quadOverdrawImage.load().Load(quadpos);

    return float4(PlasmaPalette(overdraw, 32), 1.0f);
}