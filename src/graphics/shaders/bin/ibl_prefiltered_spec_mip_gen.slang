import "../globallayout";

struct PushConstants {
    ShHandle<TextureCube> envmapTexture;
    ShHandle<SamplerState> envmapSampler;
    float roughness;
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

float hash1d(float x) {
    return frac(sin(x) * 43758.5453123);
}

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

float3 ggxImportanceSample(float2 xi, float3 n, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * float::getPi() * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float3 h = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    float3 up = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, n));
    float3 bitangent = cross(n, tangent);
    return tangent * h.x + bitangent * h.y + n * h.z;
}

float ggxDistribution(float nh, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (nh * nh) * (a2 - 1.0) + 1.0;
    return a2 / (float::getPi() * denom * denom);
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
    float3 n = computeEnvDirection(viewid, input.positionCS.xy);
    const uint samples = 4096u;

    if (s_pushConstants.roughness < 0.001f) return float4(s_pushConstants.envmapTexture.load().Sample(s_pushConstants.envmapSampler.load(), n).rgb, 1.0);

    float2 rot = float2(
        hash1d(input.positionCS.x * 12.9898 + input.positionCS.y * 78.233),
        hash1d(input.positionCS.x * 39.346 + input.positionCS.y * 11.135)
    );

    float3 accum = float3(0.0f);
    float weight = 0.0f;
    for (uint i = 0u; i < samples; i++) {
        float2 xi = hammersleySeq(i, samples);
        xi = fract(rot + xi);
        float3 h = ggxImportanceSample(xi, n, s_pushConstants.roughness);
        float3 l = normalize(2.0f * dot(n, h) * h - n);
        float nl = max(dot(n, l), 0.0f);
        if (nl > 0.0f) {
            float nh = max(dot(n, h), 0.0f);

            float pdf = ggxDistribution(nh, s_pushConstants.roughness) * nh / (4.0f * nh) + 0.0001f;
            float sample = 1.0f / (float(samples) * pdf);
            float texel = 4.0 * float::getPi() / (6.0f * s_pushConstants.cubeResolution * s_pushConstants.cubeResolution);

            float mip = max(s_pushConstants.roughness < 0.01f ? 0.0f : 0.75f * log2(sample / texel), 0.0f);

            accum += s_pushConstants.envmapTexture.load().SampleLevel(s_pushConstants.envmapSampler.load(), l, mip).rgb * nl;
            weight += nl;
        }
    }

    return float4(accum / max(weight, 0.001f), 1.0f);
}