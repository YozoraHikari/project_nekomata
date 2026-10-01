#pragma once
#include "cpp_kw_shim.h"

SH_EXPORT namespace projnekomata {

struct ShGlobalData {
    float4x4 jitteredProjview;
    float4x4 jitteredProjviewInverse;
    float4x4 projview;
    float4x4 prevProjview;
    float4x4 prevProjviewNoTranslation;
    float4x4 projviewInverse;
    float4x4 projviewNoTranslationInverse;
    float4x4 cameraModel;
    float3 cameraPos;
    float cameraNear;
    uint frameIndex;
};

}
