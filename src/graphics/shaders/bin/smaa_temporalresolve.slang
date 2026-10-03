import "../globallayout";

struct PushConstants {
    float4 rtMetrics;
    ShHandle<Texture2D> colorTexture;
    ShHandle<Texture2D> lastResolvedColorTexture;
    ShHandle<Texture2D> velocityBuffer;
    ShHandle<SamplerState> smaaLinearSampler;
    ShHandle<SamplerState> smaaNearestSampler;
};

[vk::push_constant] PushConstants s_pushConstants;

#define LinearSampler s_pushConstants.smaaLinearSampler.load()
#define PointSampler s_pushConstants.smaaNearestSampler.load()
#define SMAA_RT_METRICS s_pushConstants.rtMetrics
#include "../smaa/smaa_incl.h"

struct VSOut {
    float4 pos    : SV_Position;
    float2 uv     : TEXCOORD0;
}

[shader("vertex")]
VSOut main(uint vertexID : SV_VulkanVertexID) {
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    float2 pos = uv * 2.0f - 1.0f;

    VSOut output;
    output.pos = float4(pos, 0.0f, 1.0f);
    output.uv = uv;

    return output;
}

struct FSOut {
    float4 colorHistory : SV_Target0;
    float4 colorTransient : SV_Target1;
}

[shader("fragment")]
FSOut main(VSOut input) {
    let result = SMAAResolvePS(input.uv,
        s_pushConstants.colorTexture.load(),
        s_pushConstants.lastResolvedColorTexture.load(),
        s_pushConstants.velocityBuffer.load()
    );

    FSOut output;
    output.colorHistory = result;
    output.colorTransient = result;
    return output;
}