import "../globallayout";

struct PushConstants {
    float2 ssBegin;
    float2 ssEnd;
    float2 screenSize;
    float2 texcoordBegin;
    float2 texcoordEnd;
    ShHandle<Texture2D> texture;
    ShHandle<SamplerState> sampler;
};

[vk::push_constant] PushConstants s_pushConstants;

struct VSOut {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

[shader("vertex")]
VSOut main(uint vertexId : SV_VulkanVertexID) {
    float2 baseTexcoord = float2(vertexId & 1, vertexId >> 1);

    float2 ndcBegin = (s_pushConstants.ssBegin / s_pushConstants.screenSize) * 2.0f - 1.0f;
    float2 ndcEnd = (s_pushConstants.ssEnd / s_pushConstants.screenSize) * 2.0f - 1.0f;

    VSOut output;
    output.position = float4(lerp(ndcBegin, ndcEnd, baseTexcoord), 0.0f, 1.0f);
    output.texcoord = lerp(s_pushConstants.texcoordBegin, s_pushConstants.texcoordEnd, baseTexcoord);
    return output;
}

[shader("fragment")]
float4 main(VSOut input) : SV_Target {
    return s_pushConstants.texture.load().Sample(s_pushConstants.sampler.load(), input.texcoord);
}