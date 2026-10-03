#include "globallayout_structs.h"

struct ShHandle<T> { uint srtIndex; };

#define PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(vkDescBinding, vkDescSet, T, descArrayName) \
    [vk::binding(vkDescBinding, vkDescSet)] T descArrayName[]; \
    extension ShHandle<T> { T load() { return descArrayName[srtIndex]; } T loadNonUniform() { return descArrayName[NonUniformResourceIndex(srtIndex)]; } }

// ---- Sampled Images, float4 -----------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture1D, s_textures1D)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture2D, s_textures2D)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture3D, s_textures3D)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture1DArray, s_textures1DArray)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture2DArray, s_textures2DArray)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, TextureCube, s_texturesCube)

// ---- Sampled Images, uint4 ------------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture1D<uint4>, s_textures1D_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture2D<uint4>, s_textures2D_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture3D<uint4>, s_textures3D_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture1DArray<uint4>, s_textures1DArray_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, Texture2DArray<uint4>, s_textures2DArray_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(0, 0, TextureCube<uint4>, s_texturesCube_u32)

// ---- Storage Images, float -----------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1D<float>, s_images1D_f32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2D<float>, s_images2D_f32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture3D<float>, s_images3D_f32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1DArray<float>, s_images1DArray_f32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2DArray<float>, s_images2DArray_f32)

// ---- Storage Images, float2 -----------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1D<float2>, s_images1D_f32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2D<float2>, s_images2D_f32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture3D<float2>, s_images3D_f32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1DArray<float2>, s_images1DArray_f32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2DArray<float2>, s_images2DArray_f32x2)

// ---- Storage Images, float4 -----------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1D<float4>, s_images1D_f32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2D<float4>, s_images2D_f32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture3D<float4>, s_images3D_f32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1DArray<float4>, s_images1DArray_f32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2DArray<float4>, s_images2DArray_f32x4)

// ---- Storage Images, uint -------------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1D<uint>, s_images1D_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2D<uint>, s_images2D_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture3D<uint>, s_images3D_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1DArray<uint>, s_images1DArray_u32)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2DArray<uint>, s_images2DArray_u32)

// ---- Storage Images, uint2 ------------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1D<uint2>, s_images1D_u32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2D<uint2>, s_images2D_u32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture3D<uint2>, s_images3D_u32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1DArray<uint2>, s_images1DArray_u32x2)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2DArray<uint2>, s_images2DArray_u32x2)

// ---- Storage Images, uint4 ------------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1D<uint4>, s_images1D_u32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2D<uint4>, s_images2D_u32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture3D<uint4>, s_images3D_u32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture1DArray<uint4>, s_images1DArray_u32x4)
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(1, 0, RWTexture2DArray<uint4>, s_images2DArray_u32x4)

// ---- Samplers -------------------------------------------------------------------------------------------------------
PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE(2, 0, SamplerState, s_samplers)

#undef PROJNEKOMATA_SH_DESCRIPTOR_ARRAY_INTERFACE

// ---- Rendering Outputs ----------------------------------------------------------------------------------------------

struct DeferredRendererFSOut {
    float4 albedoAndRoughness : SV_Target0;
    float4 normalAndGeometricNormal : SV_Target1;
    float2 metallicAndAo : SV_Target2;
    float2 velocity : SV_Target3;
}