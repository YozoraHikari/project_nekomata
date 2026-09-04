import "../globallayout";
import "../normalpack/normalpack";

struct ObjectUniforms {
    float4x4 model;
    float4x4 prevModel;
    float3x3 normalMatrix;
};

struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord;
    float4 color;
};

const static uint kFlagColorIsTex = 1 << 0;
const static uint kFlagRoughnessIsTex = 1 << 1;
const static uint kFlagMetallicIsTex = 1 << 2;
const static uint kHasNormalMap = 1 << 3;
const static uint kFlagEmissiveIsTex = 1 << 4;

struct MaterialProps {
    uint flags;
    float3 colorOrTex;
    float3 emissiveOrTex;
    float roughnessOrTex;
    float metallicOrTex;
    uint normalMapTex;
};


struct PushConstants {
    ObjectUniforms* data;
    Vertex* vertexbuffer;
    projnekomata::ShGlobalData* globalData;
    MaterialProps* materialProps;
    uint* srtTableSampledImages;
    uint* srtTableSamplers;
};

[vk::push_constant] PushConstants s_pushConstants;

struct VSOut {
    float4 position : SV_Position;
    float4 mvCurrentPosition : TEXCOORD0;
    float4 mvPrevPosition : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 normal : TEXCOORD3;
    float3 tangent : TEXCOORD4;
    nointerpolation float tangentHandedness : TEXCOORD5;
}

[shader("vertex")]
VSOut main(uint vertexID: SV_VulkanVertexID) {
    float3 position = s_pushConstants.vertexbuffer[vertexID].position;
    float3 normal = s_pushConstants.vertexbuffer[vertexID].normal;
    float4 tangent = s_pushConstants.vertexbuffer[vertexID].tangent;
    float2 texcoord = s_pushConstants.vertexbuffer[vertexID].texcoord;

    float4 positionh = float4(position, 1.0f);

    float4 currentWorldspacePos = mul(s_pushConstants.data.model, positionh);
    float4 prevWorldspacePos = mul(s_pushConstants.data.prevModel, positionh);

    VSOut output;
    output.position = mul(s_pushConstants.globalData.jitteredProjview, currentWorldspacePos);
    output.mvCurrentPosition = mul(s_pushConstants.globalData.projview, currentWorldspacePos);
    output.mvPrevPosition = mul(s_pushConstants.globalData.projview, prevWorldspacePos);
    output.uv = texcoord;
    output.normal = mul(s_pushConstants.data.normalMatrix, normal);
    output.tangent = mul(float3x3(s_pushConstants.data.model), tangent.xyz);
    output.tangentHandedness = tangent.w;

    return output;
}

// ---------------------------------------------------------------------------------------------------------------------

struct FSOut {
    float4 albedoAndRoughness : SV_Target0;
    float3 emissive : SV_Target1;
    float4 normalAndGeometricNormal : SV_Target2;
    float2 metallicAndAo : SV_Target3;
    float2 velocity : SV_Target4;
}

float loadColorFromConstOrTex(float colorOrTex, float2 texcoord, bool isTex) {
    if (!isTex) return colorOrTex;
    uint texHandle = asuint(colorOrTex);
    return resolveTexture2DHandle(texHandle).Sample(resolveSamplerHandle(texHandle), texcoord).r;
}

float2 loadColorFromConstOrTex(float2 colorOrTex, float2 texcoord, bool isTex) {
    if (!isTex) return colorOrTex;
    uint texHandle = asuint(colorOrTex.x);
    return resolveTexture2DHandle(texHandle).Sample(resolveSamplerHandle(texHandle), texcoord).rg;
}

float3 loadColorFromConstOrTex(float3 colorOrTex, float2 texcoord, bool isTex) {
    if (!isTex) return colorOrTex;
    uint texHandle = asuint(colorOrTex.x);
    return resolveTexture2DHandle(texHandle).Sample(resolveSamplerHandle(texHandle), texcoord).rgb;
}

float4 loadColorFromConstOrTex(float4 colorOrTex, float2 texcoord, bool isTex) {
    if (!isTex) return colorOrTex;
    uint texHandle = asuint(colorOrTex.x);
    return resolveTexture2DHandle(texHandle).Sample(resolveSamplerHandle(texHandle), texcoord);
}

Texture2D<float4> resolveTexture2DHandle(uint handle) {
    let texImageSrtID = s_pushConstants.srtTableSampledImages[handle];
    return s_textures2D[texImageSrtID];
}
SamplerState resolveSamplerHandle(uint handle) {
    let samplerSrtID = s_pushConstants.srtTableSamplers[handle];
    return s_samplers[samplerSrtID];
}

[earlydepthstencil]
[shader("fragment")]
FSOut main(VSOut input) {
    let props = *s_pushConstants.materialProps;
    float3 albedo   = loadColorFromConstOrTex(props.colorOrTex, input.uv, props.flags & kFlagColorIsTex);
    float3 emissive = loadColorFromConstOrTex(props.emissiveOrTex, input.uv, props.flags & kFlagEmissiveIsTex);
    float roughness = loadColorFromConstOrTex(props.roughnessOrTex, input.uv, props.flags & kFlagRoughnessIsTex);
    float metallic  = loadColorFromConstOrTex(props.metallicOrTex, input.uv, props.flags & kFlagMetallicIsTex);

    float ao = 1.0f;

    float3 n = normalize(input.normal);

    float3 norm;
    if (props.flags & kHasNormalMap) {
        float3 t = normalize(input.tangent);
        float3 b = cross(n, t) * input.tangentHandedness;
        float3x3 tbn = float3x3(t, b, n);
        float2 nm = resolveTexture2DHandle(props.normalMapTex).Sample(resolveSamplerHandle(props.normalMapTex), input.uv).rg * 2.0f - 1.0f;
        float3 mappednorm = float3(nm, sqrt(saturate(1.0f - dot(nm, nm))));

        norm = normalize(mul(mappednorm, tbn));
    } else {
        norm = n;
    }

    float2 pn = octahedronNormalPack(norm);
    float2 pgn = octahedronNormalPack(n);

    float2 ndcCurrent = input.mvCurrentPosition.xy / input.mvCurrentPosition.w;
    float2 ndcPrev    = input.mvPrevPosition.xy / input.mvPrevPosition.w;
    float2 velocity = (ndcCurrent - ndcPrev) * 0.5f;

/*
    float2 pixelpos = input.position.xy - 0.5f;
    uint2 quadpos = uint2(floor(pixelpos / 2.0f));

    // ---- Determine the quad leader lane
    uint4 activeLanes = WaveActiveBallot(!IsHelperLane());
    uint lane = WaveGetLaneIndex();
    uint laneSubword = lane & 31;
    uint quadIdInSubword = (lane >> 2) & 7;
    uint quadSubwordMask = 0b1111 << (quadIdInSubword << 2);
    uint activeLaneSubword =
        (lane < 32 ? activeLanes.x : (lane < 64 ? activeLanes.y : (lane < 96 ? activeLanes.z : activeLanes.w)));
    uint quadActiveMask = activeLaneSubword & quadSubwordMask;
    uint leaderLane = firstbitlow(quadActiveMask);
    bool isQuadLeader = laneSubword == leaderLane;

    uint helpersCount = countbits(~activeLaneSubword & quadSubwordMask);

    if (isQuadLeader) {
        let image = s_images2D_u32[s_pushConstants.overdrawImageSrtID];
        InterlockedAdd(image[quadpos], helpersCount);
    }
*/
    FSOut output;
    output.albedoAndRoughness = float4(albedo, roughness);
    output.emissive = emissive;
    output.normalAndGeometricNormal = float4(pn, pgn);
    output.metallicAndAo = float2(metallic, ao);
    output.velocity = velocity;

    return output;
}