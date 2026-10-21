import "../globallayout";

struct ObjectUniforms {
    float3x4 model;
    float3x4 prevModel;
    float3x3 normalMatrix;
};

struct ShadowmapDescriptor {
    float4x4 lightProjview;
    float2 atlasOffset;
    float2 atlasRectSize;
};

struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord;
    float4 color;
};

struct PushConstants {
    ObjectUniforms* data;
    ShadowmapDescriptor* lightShadowRenderingDescriptor;
    Vertex* vertexbuffer;
    projnekomata::ShGlobalData* globalData;
}

[vk::push_constant] PushConstants s_pushConstants;

struct VSIn {
    [[vk::location(0)]] float3 position;
};

struct VSOut {
    float4 position : SV_Position;
};

[shader("vertex")]
VSOut main(VSIn input) {
    float4 pos = float4(input.position, 1.0f);
    float4 worldspacePos = float4(mul(s_pushConstants.data.model, pos), 1.0f);

    VSOut output;
    output.position = mul(s_pushConstants.lightShadowRenderingDescriptor.lightProjview, worldspacePos);

    return output;
}