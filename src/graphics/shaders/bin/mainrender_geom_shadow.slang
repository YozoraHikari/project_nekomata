import "../globallayout";

struct ObjectUniforms {
    float4x4 model;
    float4x4 prevModel;
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

struct VSOut {
    float4 position : SV_Position;
};

[shader("vertex")]
VSOut main(uint vertexID : SV_VulkanVertexID) {
    float3 position = s_pushConstants.vertexbuffer[vertexID].position;

    float4 worldspacePos = mul(s_pushConstants.data.model, float4(position, 1.0));

    VSOut output;
    output.position = mul(s_pushConstants.lightShadowRenderingDescriptor.lightProjview, worldspacePos);

    return output;
}