import "../globallayout";

struct PushConstants {
    float2 resolution;
    float2 texelSize;
    uint srcTextureSrtID;
    uint linearSamplerSrtID;
    uint dstStorageImageSrtID;
    uint isFirstMip;
};

[vk::push_constant] PushConstants pushConstants;

float luma(float3 c) {
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 karisAverage(float3 c0, float3 c1, float3 c2, float3 c3) {
    float w0 = 1.0 / (1.0 + luma(c0));
    float w1 = 1.0 / (1.0 + luma(c1));
    float w2 = 1.0 / (1.0 + luma(c2));
    float w3 = 1.0 / (1.0 + luma(c3));
    return (c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3) / max(w0 + w1 + w2 + w3, 1e-5);
}

float3 sampleTap(float2 texcoord, float2 offset) {
    return s_textures2D[pushConstants.srcTextureSrtID].SampleLevel(s_samplers[pushConstants.linearSamplerSrtID], texcoord + offset * pushConstants.texelSize, 0).rgb;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    float2 pixelcoord = float2(dtid.xy);

    if (any(pixelcoord >= pushConstants.resolution)) return;
    float2 texcoord = (pixelcoord + 0.5f) / float2(pushConstants.resolution);

    float3 c0 = sampleTap(texcoord, float2(-1.0f, -1.0f));
    float3 c1 = sampleTap(texcoord, float2(1.0f, -1.0f));
    float3 c2 = sampleTap(texcoord, float2(-1.0f, 1.0f));
    float3 c3 = sampleTap(texcoord, float2(1.0f, 1.0f));

    float3 r0 = sampleTap(texcoord, float2(0.0f, -2.0f));
    float3 r1 = sampleTap(texcoord, float2(-2.0f, 0.0f));
    float3 r2 = sampleTap(texcoord, float2(2.0f, 0.0f));
    float3 r3 = sampleTap(texcoord, float2(0.0f, 2.0f));

    float3 r4 = sampleTap(texcoord, float2(-2.0f, -2.0f));
    float3 r5 = sampleTap(texcoord, float2(2.0f, -2.0f));
    float3 r6 = sampleTap(texcoord, float2(-2.0f, 2.0f));
    float3 r7 = sampleTap(texcoord, float2(2.0f, 2.0f));

    float3 center = sampleTap(texcoord, float2(0.0f, 0.0f));

    float3 result;
    if (pushConstants.isFirstMip) {
        float3 q0 = karisAverage(r0, r1, r4, center);
        float3 q1 = karisAverage(r0, r2, r5, center);
        float3 q2 = karisAverage(r1, r3, r6, center);
        float3 q3 = karisAverage(r2, r3, r7, center);
        float3 qc = karisAverage(c0, c1, c2, c3);

        result = qc * 0.5f + (q0 + q1 + q2 + q3) * 0.125f;
    } else {
        result = center * 0.125f
            + (c0 + c1 + c2 + c3) * 0.125f
            + (r0 + r1 + r2 + r3) * 0.0625f
            + (r4 + r5 + r6 + r7) * 0.03125f;
    }

    result = max(result, 0.0001f);

    s_images2D_f32x4[pushConstants.dstStorageImageSrtID].Store(pixelcoord, float4(result, 1.0f));
}