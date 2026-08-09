import "../globallayout";
import "../normalpack/normalpack";

struct Pointlight {
    float3 position;
    float3 lightRadiance;
};

struct PushConstants {
    projnekomata::ShGlobalData* globalData;
    Pointlight* pointlights;
    uint pointlightCount;
    uint depthTextureSrtID;
    uint albedoAndRoughnessTextureSrtID;
    uint normalTextureSrtID;
    uint metallicAoTextureSrtID;
    uint skyboxTextureSrtID;
    uint irradianceTextureSrtID;
    uint prefiltTextureSrtID;
    uint iblLutTextureSrtID;
    uint linearSamplerSrtID;
    uint nearestSamplerSrtID;
};

[vk::push_constant] PushConstants s_pushConstants;

struct VSOut {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float2 positionCS : TEXCOORD1;
}

[shader("vertex")]
VSOut main(uint vertexId : SV_VulkanVertexID) {
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float2 pos = uv * 2.0f - 1.0f;

    VSOut output;
    output.position = float4(pos, 0.0f, 1.0f);
    output.texcoord = uv;
    output.positionCS = pos;
    return output;
}

// ---------------------------------------------------------------------------------------------------------------------

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

float3 reconstructPositionWS(float2 ndc, float depth) {
    float4 positionSS = float4(ndc, depth, 1.0);
    float4 positionWS = mul(s_pushConstants.globalData.projviewInverse, positionSS);
    return positionWS.xyz / positionWS.w;
}

float3 reconstructViewdir(float2 ndc) {
    const float near = 0.0001;

    float4 nearClip = float4(ndc, near, 1.0);
    float4 worldNear = mul(s_pushConstants.globalData.projviewInverse, nearClip);

    worldNear.xyz /= worldNear.w;

    float3 viewDir = normalize(worldNear.xyz - s_pushConstants.globalData.cameraPos);

    return viewDir;
}

float3 fresnelSchlickRoughnessApprox(float costheta, float3 f0, float roughness) {
    return f0 + (max(float3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - costheta, 0.0, 1.0), 5.0);
}
float3 fresnelSchlick(float cos_theta, float3 f0) {
    return f0 + (float3(1.0) - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float ggxDist(float nh, float roughness) {
    float nh2 = nh * nh;

    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float denomPrec = nh2 * (alpha2 - 1.0) + 1.0;
    float denom = float::getPi() * denomPrec * denomPrec;

    return alpha2 / denom;
}

float heightCorrelatedSmithVisibilityTerm(float nv, float nl, float roughness) {
    float alpha = roughness * roughness;

    float lambdav = nl * sqrt(alpha + (1.0f - alpha) * nv * nv);
    float lambdal = nv * sqrt(alpha + (1.0f - alpha) * nl * nl);

    return 0.5f / max(lambdav + lambdal, 0.0001f);
}

float3 brdf(float3 norm, float3 viewdir, float3 lightdir, float nl, float3 albedo, float metallic, float roughness) {
    float3 f0 = lerp(float3(0.04), albedo, metallic);

    float3 halfvec = normalize(viewdir + lightdir);
    float  nv = max(dot(norm, viewdir), 0.0001);
    float  nh = max(dot(norm, halfvec), 0.0);
    float  vh = max(dot(viewdir, halfvec), 0.0);
    float  lh = max(dot(lightdir, halfvec), 0.0);

    // ---- for f_CT = DGF / (4 * nv * nl) ---------------------------------------------------------------------------------------------------------------------
    float  ndf  = ggxDist(nh, roughness);
    float  gvis = heightCorrelatedSmithVisibilityTerm(nv, nl, roughness); // = D / (4 * nv * nl)
    float3 f    = fresnelSchlick(vh, f0);
    float3 specular = (ndf * gvis * f);

    // ---- Energy terms ---------------------------------------------------------------------------------------------------------------------------------------
    float3 kS = f;
    float3 kD = (float3(1.0) - kS) * (1.0 - metallic);

    // ---- Diffuse term ---------------------------------------------------------------------------------------------------------------------------------------
    float fd90 = 0.5 + 2.0 * lh * lh * roughness;

    float lightScatter = 1.0 + (fd90 - 1.0) * pow(1.0 - nl, 5);
    float viewScatter = 1.0 + (fd90 - 1.0) * pow(1.0 - nv, 5);

    float3 diffuse = kD * albedo * lightScatter * viewScatter / float::getPi();

    return diffuse + specular;
}

float3 computeLightContribution(float3 norm, float3 viewdir, float3 lightdir, float3 radiance, float3 albedo, float metallic, float roughness) {
    float nl = max(dot(norm, lightdir), 0.0001);
    float3 brdfterm = brdf(norm, viewdir, lightdir, nl, albedo, metallic, roughness);
    return brdfterm * radiance * nl;
}

const static float roughnessMaxLod = 6.0f;
float3 computePixelColor(float3 fragmentPosWS, float3 norm, float3 geometricNorm, float3 albedo, float metallic, float roughness, float ao) {
    float3 viewdir = normalize(s_pushConstants.globalData.cameraPos - fragmentPosWS);
    float3 accum = float3(0.0);

    // ---- The static pointlights
    for (uint i = 0; i < s_pushConstants.pointlightCount; i++) {
        let light = s_pushConstants.pointlights[i];
        float3 lz = light.position - fragmentPosWS;
        float3 l = normalize(lz);
        float ngl = dot(geometricNorm, l);
        float lighthorizon = clamp(ngl / 0.1f, 0.0f, 1.0f);
        float3 ld = length(lz);
        float3 radiance = light.lightRadiance / (ld * ld);
        accum += computeLightContribution(norm, viewdir, l, radiance, albedo, metallic, roughness) * lighthorizon;
    }

    // ---- Diffuse IBL ------------------------------------------------------------------------------------------------
    float3 irradiance = s_texturesCube[s_pushConstants.irradianceTextureSrtID].Sample(s_samplers[s_pushConstants.linearSamplerSrtID], norm).rgb;
    float3 diffuseibl = irradiance * albedo;

    // ---- Specular IBL -----------------------------------------------------------------------------------------------
    float nv = max(dot(norm, viewdir), 0.0f);
    float3 r = reflect(-viewdir, norm);
    float horizon = clamp(1.0f + 1.5f * dot(r, geometricNorm), 0.0f, 1.0f);
    float hbsoOcclusion = horizon * horizon;

    float3 prefiltered = s_texturesCube[s_pushConstants.prefiltTextureSrtID].SampleLevel(s_samplers[s_pushConstants.linearSamplerSrtID], r, roughness * roughnessMaxLod).rgb;
    float2 envbrdf = s_textures2D[s_pushConstants.iblLutTextureSrtID].Sample(s_samplers[s_pushConstants.linearSamplerSrtID], float2(nv, roughness)).rg;

    float3 f0 = lerp(float3(0.04), albedo, metallic);
    float3 fibl = fresnelSchlickRoughnessApprox(nv, f0, roughness);
    float3 kdibl = (float3(1.0) - fibl) * (1.0 - metallic);

    float3 specularibl = prefiltered * (fibl * envbrdf.x + envbrdf.y) * hbsoOcclusion;

    float3 ambient = (kdibl * diffuseibl + specularibl) * ao;

    return ambient + accum;
}

[shader("fragment")]
float4 main(VSOut input) : SV_Target {
    let depth = s_textures2D[s_pushConstants.depthTextureSrtID].SampleLevel(s_samplers[s_pushConstants.nearestSamplerSrtID], input.texcoord, 0).r;
    let albedoAndRoughness = s_textures2D[s_pushConstants.albedoAndRoughnessTextureSrtID].SampleLevel(s_samplers[s_pushConstants.nearestSamplerSrtID], input.texcoord, 0);
    let pn = s_textures2D[s_pushConstants.normalTextureSrtID].SampleLevel(s_samplers[s_pushConstants.nearestSamplerSrtID], input.texcoord, 0);
    let metallicAo = s_textures2D[s_pushConstants.metallicAoTextureSrtID].SampleLevel(s_samplers[s_pushConstants.nearestSamplerSrtID], input.texcoord, 0).xy;

    const float ev = -2.0f;
    const float exposure = pow(2.0f, ev);

    float3 viewdir = reconstructViewdir(input.positionCS.xy);
    float3 vddx = ddx(viewdir);
    float3 vddy = ddy(viewdir);

    if (depth == 0.0) {
        float3 col = s_texturesCube[s_pushConstants.skyboxTextureSrtID].SampleGrad(s_samplers[s_pushConstants.linearSamplerSrtID], viewdir, vddx, vddy).rgb * exposure;
        return float4(AcesTonemap::map(col), 1.0);
    }

    float3 position = reconstructPositionWS(input.positionCS, depth);
    float3 albedo = albedoAndRoughness.rgb;
    float3 n = octahedronNormalUnpack(pn.xy);
    float3 geometricN = octahedronNormalUnpack(pn.zw);

    float roughness = albedoAndRoughness.a;
    float metallic = metallicAo.r;
    float ao = metallicAo.g;

    float3 color = computePixelColor(position, n, geometricN, albedo, metallic, roughness, ao) * exposure;
    float3 tonemapped = AcesTonemap::map(color);
    tonemapped += (interleavedGradientNoise(input.position.xy) - 0.5) / 255.0;

    return float4(tonemapped, 1.0);
}