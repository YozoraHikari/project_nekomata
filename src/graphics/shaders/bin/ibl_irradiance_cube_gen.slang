import "../globallayout";

struct PushConstants {
    ShHandle<TextureCube> envmapTexture;
    ShHandle<SamplerState> envmapSampler;
    float cubeResolution;
};

[vk::push_constant] PushConstants s_pushConstants;

struct VSOut {
    float4 position : SV_Position;
    float2 positionCS : TEXCOORD0;
}

[shader("vertex")]
VSOut main(uint vertexId : SV_VulkanVertexID) {
    float2 pos = float2((vertexId << 1) & 2, vertexId & 2) * 2.0f - 1.0f;

    VSOut output;
    output.position = float4(pos, 0.0f, 1.0f);
    output.positionCS = pos;
    return output;
}

// ---------------------------------------------------------------------------------------------------------------------

uint revbits32(uint x) {
    x = ((x & 0x55555555u) << 1) | ((x & 0xaaaaaaaau) >> 1);
    x = ((x & 0x33333333u) << 2) | ((x & 0xccccccccu) >> 2);
    x = ((x & 0x0f0f0f0fu) << 4) | ((x & 0xf0f0f0f0u) >> 4);
    x = ((x & 0x00ff00ffu) << 8) | ((x & 0xff00ff00u) >> 8);
    return (x << 16) | (x >> 16);
}

float radicalInverse(uint x) {
    return revbits32(x) * 2.3283064365386963e-10f; // = 0x100000000
}

float2 hammersleySeq(uint i, uint n) {
    return float2(float(i) / float(n), radicalInverse(i));
}

float3 getHemisphereSampleDirection(float2 xi) {
    let phi = 2.0f * float::getPi() * xi.x;
    let cosTheta = sqrt(1.0f - xi.y);
    let sinTheta = sqrt(xi.y);
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3 computeEnvDirection(uint viewid, float2 position) {
    float3 pdirection;
    switch (viewid) {
        case 0: pdirection = float3( 1.0f, -position.y, -position.x); break; // +X
        case 1: pdirection = float3(-1.0f, -position.y,  position.x); break; // -X
        case 2: pdirection = float3( position.x,  1.0f,  position.y); break; // +Y
        case 3: pdirection = float3( position.x, -1.0f, -position.y); break; // -Y
        case 4: pdirection = float3( position.x, -position.y,  1.0f); break; // +Z
        case 5: pdirection = float3(-position.x, -position.y, -1.0f); break; // -Z
    }
    return normalize(pdirection);
}

[shader("fragment")]
float4 main(VSOut input, uint viewid : SV_ViewID) : SV_Target {
    let n = computeEnvDirection(viewid, input.positionCS.xy);
    let nsign = n.z >= 0.0f ? 1.0f : -1.0f;
    let a = -1.0f / (nsign + n.z);
    let b = n.x * n.y * a;
    float3 tgx = float3(1.0f + nsign * n.x * n.x * a, nsign * b, -nsign * n.x);
    float3 tgy = float3(b, nsign + n.y * n.y * a, -n.y);

    var irradiance = float3(0.0f);
    const uint samples = 16384u;

    for (uint i = 0u; i < samples; i++) {
        float2 xi = hammersleySeq(i, samples);
        float3 sampleTangent = getHemisphereSampleDirection(xi);
        float3 sampleDirection = sampleTangent.x * tgx + sampleTangent.y * tgy + sampleTangent.z * n;
        irradiance += s_pushConstants.envmapTexture.load().Sample(s_pushConstants.envmapSampler.load(), sampleDirection).rgb;
    }

    let finalIrradiance = irradiance / float(samples);

    return float4(finalIrradiance, 1.0f);
}