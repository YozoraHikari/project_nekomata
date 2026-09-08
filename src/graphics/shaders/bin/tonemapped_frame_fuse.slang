import "../globallayout";

struct PushConstants {
    projnekomata::ShGlobalData* globalData;
    uint nearestSamplerSrtID;
    uint hdrColorBufferSrtID;
    uint filteredBloomImageSrtID;
    float bloomStrength;
};

[vk::push_constant] PushConstants s_pushConstants;

struct VSOut {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

[shader("vertex")]
VSOut main(uint vertexId : SV_VulkanVertexID) {
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float2 pos = uv * 2.0f - 1.0f;

    VSOut output;
    output.position = float4(pos, 0.0f, 1.0f);
    output.texcoord = uv;
    return output;
}

namespace AcesTonemap {
    func map(x: float3) -> float3 {
        const float a = 2.51;
        const float b = 0.03;
        const float c = 2.43;
        const float d = 0.59;
        const float e = 0.14;
        return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
    }
}

float interleavedGradientNoise(float2 svp) {
    svp += 0.618033988749895 * float2(49.0, 17.0) * float(s_pushConstants.globalData.frameIndex % 64);
    const float3 d = float3(0.06711056, 0.00583715, 52.9829189);
    return fract(d.z * fract(dot(svp, d.xy)));
}

[shader("fragment")]
float4 main(VSOut input) : SV_Target {
    let hdrSceneColor = s_textures2D[s_pushConstants.hdrColorBufferSrtID].SampleLevel(s_samplers[s_pushConstants.nearestSamplerSrtID], input.texcoord, 0).rgb;
    let filteredBloomColor = s_textures2D[s_pushConstants.filteredBloomImageSrtID].SampleLevel(s_samplers[s_pushConstants.nearestSamplerSrtID], input.texcoord, 0).rgb;

    let fusedColor = lerp(hdrSceneColor, filteredBloomColor, s_pushConstants.bloomStrength);

    float3 tonemapped = AcesTonemap::map(fusedColor);
    tonemapped += (interleavedGradientNoise(input.position.xy) - 0.5f) / 255.0f;

    return float4(tonemapped, 1.0f);
}