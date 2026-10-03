import "../globallayout";

struct PushConstants {
    float4 rtMetrics;
    ShHandle<Texture2D> colorTexture;
    ShHandle<SamplerState> smaaLinearSampler;
    ShHandle<SamplerState> smaaNearestSampler;
};

[vk::push_constant] PushConstants s_pushConstants;

#define LinearSampler s_pushConstants.smaaLinearSampler.load()
#define PointSampler s_pushConstants.smaaNearestSampler.load()
#define SMAA_RT_METRICS s_pushConstants.rtMetrics
#include "../smaa/smaa_incl.h"

struct VSOut {
    float4 pos        : SV_Position;
    float2 uv         : TEXCOORD0;
    float4 offsets[3] : TEXCOORD1;
}

[shader("vertex")]
VSOut main(uint vertexID : SV_VulkanVertexID) {
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    float2 pos = uv * 2.0f - 1.0f;

    VSOut output;
    output.pos = float4(pos, 0.0f, 1.0f);
    output.uv = uv;

    SMAAEdgeDetectionVS(uv, output.offsets);

    return output;
}

[shader("fragment")]
float2 main(VSOut input) : SV_Target {
    let colorTex = s_pushConstants.colorTexture.load();

    return SMAAColorEdgeDetectionPS(input.uv, input.offsets, colorTex);
}