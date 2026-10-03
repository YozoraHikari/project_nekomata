import "../globallayout";

struct PushConstants {
    float2 resolution;
    ShHandle<Texture2D> smallerMipSampledImage;
    ShHandle<SamplerState> linearSampler;
    ShHandle<RWTexture2D<float4>> dstMipStorageImage;
    float filterRadius;
};

[vk::push_constant] PushConstants s_pushConstants;

float3 sampleTap(float2 texcoord, float2 offset) {
    return s_pushConstants.smallerMipSampledImage.load().SampleLevel(s_pushConstants.linearSampler.load(), texcoord + offset, 0).rgb;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    float2 pixelcoord = float2(dtid.xy);

    if (any(pixelcoord >= s_pushConstants.resolution)) return;

    float2 texcoord = (pixelcoord + 0.5f) / float2(s_pushConstants.resolution);
    float  radius = s_pushConstants.filterRadius;

    float3 m00 = sampleTap(texcoord, float2(-radius, -radius));
    float3 m01 = sampleTap(texcoord, float2( 0.0f,   -radius));
    float3 m02 = sampleTap(texcoord, float2( radius, -radius));
    float3 m10 = sampleTap(texcoord, float2(-radius,  0.0f  ));
    float3 m11 = sampleTap(texcoord, float2( 0.0f,    0.0f  ));
    float3 m12 = sampleTap(texcoord, float2( radius,  0.0f  ));
    float3 m20 = sampleTap(texcoord, float2(-radius,  radius));
    float3 m21 = sampleTap(texcoord, float2( 0.0f,    radius));
    float3 m22 = sampleTap(texcoord, float2( radius,  radius));

    float3 previous = s_pushConstants.dstMipStorageImage.load().Load(dtid.xy).rgb;
    float3 upsampled = previous
        + m11 * 4.0f
        + (m01 + m10 + m12 + m21) * 2.0f
        + (m00 + m02 + m20 + m22);
    upsampled /= 16.0f;

    s_pushConstants.dstMipStorageImage.load().Store(dtid.xy, float4(upsampled, 1.0f));
}