module;
#include <cstddef>
module projnekomata;
import vulkan;
import :graphics.texturesystem.texture_manager;
import :graphics.vulkan.spv_shader_code;
import :graphics.fontsystem.font_manager;
import :graphics.rendering.shared_rendering_resources;
import :graphics.cmd_alloc;
import :graphics.vulkan.vk_commands_barriers;
import :graphics.materialsystem.mat_manager;
import :graphics.rendering.smaa_consts;

namespace projnekomata::gfx {

constexpr u32 prefilterImageSize = 512;
constexpr u32 prefilterImageMips = std::bit_width(prefilterImageSize) - 3;

SharedRenderingResources::SharedRenderingResources(std::nullptr_t) {}
SharedRenderingResources::SharedRenderingResources() {

    auto samplerParams = SamplerParams::defaultValues()
        .setMinFilter(vk::Filter::eLinear)
        .setMagFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear);

    m_skyCubemap = TextureManager::get().loadKtx2TextureBlocking(
        "//assets:/sky.ktx2",
        samplerParams
    );

    m_skyIrradianceCubemap = TextureManager::get().createTexture(
        32, 32, 1, 1, 1, true,
        vk::Format::eB10G11R11UfloatPack32,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        samplerParams
    );

    m_skyPrefilterCubemap = TextureManager::get().createTexture(
        prefilterImageSize, prefilterImageSize, 1, 1, prefilterImageMips, true,
        vk::Format::eB10G11R11UfloatPack32,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        samplerParams
    );

    m_brdfLUT = TextureManager::get().loadKtx2TextureBlocking(
        "//assets:/ibllut.ktx2",
        samplerParams
    );

    m_smaaAreaTexture = TextureManager::get().loadKtx2TextureBlocking(
        "//assets:/smaa_areatex.ktx2",
        samplerParams
    );

    m_smaaSearchTexture = TextureManager::get().loadKtx2TextureBlocking(
        "//assets:/smaa_searchtex.ktx2",
        samplerParams
    );

    auto iblIrradianceGenShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/ibl_irradiance_cube_gen.spv").unwrap();
    auto iblPrefilterGenShader  = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/ibl_prefiltered_spec_mip_gen.spv").unwrap();

    m_iblIrradianceCubeGeneratorLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 12, vk::ShaderStageFlagBits::eFragment)
        .build();
    m_iblIrradianceCubeGeneratorPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_iblIrradianceCubeGeneratorLayout)
        .addShader(iblIrradianceGenShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(iblIrradianceGenShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
            vk::PipelineColorBlendAttachmentState{}
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eB10G11R11UfloatPack32
        )
        .setMultiviewViewsMask(0b111111)
        .build();

    m_iblPrefilterCubeGeneratorLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 16, vk::ShaderStageFlagBits::eFragment)
        .build();
    m_iblPrefilterCubeGeneratorPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_iblPrefilterCubeGeneratorLayout)
        .addShader(iblPrefilterGenShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(iblPrefilterGenShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
            vk::PipelineColorBlendAttachmentState{}
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eB10G11R11UfloatPack32
        )
        .setMultiviewViewsMask(0b111111)
        .build();

    m_mainLightingPassLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        // .addDescriptorSetLayout(m_subpassInputAttachmentsDescriptorSetLayout)
        .addPushConstantRange(
            0, 64,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        )
        .build();
    auto lightingPassShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/mainrender_lighting.spv").unwrap();
    m_mainLightingPassPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_mainLightingPassLayout)
        .addShader(lightingPassShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(lightingPassShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
        // .pushInputAttachment(vk::Format::eR8G8B8A8Unorm) // Albedo + Roughness
        // .pushInputAttachment(vk::Format::eR16G16B16A16Snorm) // Normals
        // .pushInputAttachment(vk::Format::eR8G8Unorm) // Metallic + AO
        // .pushInputAttachment(vk::Format::eR16G16Sfloat) // Motion Vectors
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR8G8B8A8Srgb
        ) // Color
        // .setRenderingAttachmentLocations(Slice<const u32>(deferredLightingStageLocations, 5))
        // .setRenderingInputAttachmentIndices(Slice<const u32>(deferredLightingStageInputLocs, 5), &deferredLightingStageDepthInputLoc)
        .build();

    m_bitmapFontRendererLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 48, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto bitmapFontRendererShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/bitmap_font.spv").unwrap();
    m_bitmapFontRendererPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_bitmapFontRendererLayout)
        .addShader(bitmapFontRendererShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(bitmapFontRendererShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleStrip)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
                    .setBlendEnable(true)
                    .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                    .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                    .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setColorBlendOp(vk::BlendOp::eAdd)
                    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                    .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setAlphaBlendOp(vk::BlendOp::eAdd),
                vk::Format::eR8G8B8A8Srgb
        )
        .build();

    m_uiRectRendererLayout = vkrhi::VulkanPipelineLayout::builder()
        .addPushConstantRange(0, 40, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto uiRectRendererShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/ui_rect.spv").unwrap();
    m_uiRectRendererPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_uiRectRendererLayout)
        .addShader(uiRectRendererShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(uiRectRendererShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleStrip)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
                    .setBlendEnable(true)
                    .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                    .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                    .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setColorBlendOp(vk::BlendOp::eAdd)
                    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                    .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setAlphaBlendOp(vk::BlendOp::eAdd),
            vk::Format::eR8G8B8A8Srgb
        )
    .build();

    m_uiTextureRendererLayout = vkrhi::VulkanPipelineLayout::builder()
        .addPushConstantRange(0, 48, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .build();
    auto uiTextureRendererShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/ui_texture.spv").unwrap();
    m_uiTextureRendererPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_uiTextureRendererLayout)
        .addShader(uiTextureRendererShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(uiTextureRendererShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleStrip)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
                    .setBlendEnable(true)
                    .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                    .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                    .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setColorBlendOp(vk::BlendOp::eAdd)
                    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                    .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setAlphaBlendOp(vk::BlendOp::eAdd),
            vk::Format::eR8G8B8A8Srgb
        )
        .build();

    // ---- SMAA -----------------------------------------------------------------------------------------------------------------------------------------------

    buildSmaaPipelines();

    m_velbufferBgLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 8, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto velbufferBgShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/velbuffer_bg.spv").unwrap();
    m_velbufferBgPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_velbufferBgLayout)
        .addShader(velbufferBgShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(velbufferBgShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
        // .pushUnusedAttachment(vk::Format::eR8G8B8A8Unorm) // Albedo + Roughness
        // .pushUnusedAttachment(vk::Format::eR16G16B16A16Snorm) // Normals
        // .pushUnusedAttachment(vk::Format::eR8G8Unorm) // Metallic + AO
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR16G16Sfloat
        )
        // .pushUnusedAttachment(vk::Format::eR8G8B8A8Srgb) // Color
        // .setRenderingAttachmentLocations(Slice<const u32>(clearVelbufferLocations, 5))
        // .setRenderingInputAttachmentIndices(Slice<const u32>(inputLocationsAllUnused, 5), nullptr)
        .build();

    m_quadOverdrawVisLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 4, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto quadOverdrawVisShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/quad_overdraw_vis.spv").unwrap();
    m_quadOverdrawVisPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_quadOverdrawVisLayout)
        .addShader(quadOverdrawVisShader, vk::ShaderStageFlagBits::eVertex)
        .addShader(quadOverdrawVisShader, vk::ShaderStageFlagBits::eFragment)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
            vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR8G8B8A8Srgb
        )
        .build();

    buildIblSecondaryCubemaps();
}

static auto& cvRdSmaaQuality = CvarManager::get().registerCvar<u32>("rd.smaaqlevel", 3);

auto SharedRenderingResources::checkGraphicsSettingsAndMaybeRecompileShaders() -> void {
    if (m_compiledSmaaPreset != cvRdSmaaQuality.get()) {
        buildSmaaPipelines();
    }
}

auto SharedRenderingResources::refitHysteresisStates(usize renderableSparseCount) -> void {
    if (m_meshHysteresisStates.len() < renderableSparseCount) {
        m_meshHysteresisStates.resize(renderableSparseCount, MeshHysteresisState());
    }
    if (m_lastRenderableModelMatrices.len() < renderableSparseCount) {
        m_lastRenderableModelMatrices.resize(renderableSparseCount, Matrix4x4f::identity());
    }
}


auto mapSmaaQualityToPreset(u32 val) -> SmaaSpecializationConsts& {
    switch (val) {
        case 0: return SmaaSpecializationConstantPresets::kLow;
        case 1: return SmaaSpecializationConstantPresets::kMedium;
        case 2: return SmaaSpecializationConstantPresets::kHigh;
        case 3: return SmaaSpecializationConstantPresets::kUltra;
        default: return SmaaSpecializationConstantPresets::kLow;
    }
}

auto SharedRenderingResources::buildSmaaPipelines() -> void {
    auto smaaQuality = cvRdSmaaQuality.get();
    auto smaaPreset = mapSmaaQualityToPreset(smaaQuality);

    m_compiledSmaaPreset = smaaQuality;

    m_smaaEdgeDetectLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 28, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto smaaEdgeDetectShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/smaa_edgedetect.spv").unwrap();
    m_smaaEdgeDetectPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_smaaEdgeDetectLayout)
        .addShader(smaaEdgeDetectShader, vk::ShaderStageFlagBits::eVertex, smaaPreset)
        .addShader(smaaEdgeDetectShader, vk::ShaderStageFlagBits::eFragment, smaaPreset)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR8G8Unorm
        )
        .build();

    m_smaaBlendWeightLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 40, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto smaaBlendWeightShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/smaa_blendweight.spv").unwrap();
    m_smaaBlendWeightPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_smaaBlendWeightLayout)
        .addShader(smaaBlendWeightShader, vk::ShaderStageFlagBits::eVertex, smaaPreset)
        .addShader(smaaBlendWeightShader, vk::ShaderStageFlagBits::eFragment, smaaPreset)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR8G8B8A8Unorm
        )
        .build();

    m_smaaNeighborhoodBlendLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 36, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto smaaNeighborhoodBlendShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/smaa_neighborhoodblend.spv").unwrap();
    m_smaaNeighborhoodBlendPipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_smaaNeighborhoodBlendLayout)
        .addShader(smaaNeighborhoodBlendShader, vk::ShaderStageFlagBits::eVertex, smaaPreset)
        .addShader(smaaNeighborhoodBlendShader, vk::ShaderStageFlagBits::eFragment, smaaPreset)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR8G8B8A8Srgb
        )
        .build();

    m_smaaTemporalResolveLayout = vkrhi::VulkanPipelineLayout::builder()
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .addPushConstantRange(0, 36, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .build();
    auto smaaTemporalResolveShader = vkrhi::SpirvShaderCode::loadFromFile("//spirv:/smaa_temporalresolve.spv").unwrap();
    m_smaaTemporalResolvePipeline = vkrhi::VulkanGraphicsPipeline::builder()
        .setPipelineLayout(m_smaaTemporalResolveLayout)
        .addShader(smaaTemporalResolveShader, vk::ShaderStageFlagBits::eVertex, smaaPreset)
        .addShader(smaaTemporalResolveShader, vk::ShaderStageFlagBits::eFragment, smaaPreset)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setRastPolygonMode(vk::PolygonMode::eFill)
        .setRastCulling(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .setRastLineWidth(1.0f)
        .disableMultisampling()
        .disableDepthTest()
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR8G8B8A8Unorm
        )
        .pushRenderingAttachment(
        vk::PipelineColorBlendAttachmentState{}
             .setBlendEnable(false)
             .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR8G8B8A8Unorm
        )
        .build();

}

auto SharedRenderingResources::buildIblSecondaryCubemaps() -> void {
    auto& irradianceImage = TextureManager::get().getTextureResources(m_skyIrradianceCubemap).image();
    auto& prefilterImage = TextureManager::get().getTextureResources(m_skyPrefilterCubemap).image();
    f32 cubeResolution = static_cast<f32>(TextureManager::get().getTextureResources(m_skyCubemap).image().extent().width);

    auto irradianceImageArrayView = irradianceImage.createImageView(
        0, 1, 0, 6, false
    );

    u32 sampler = TextureManager::get().samplerCache().acquireSampler(
        SamplerParams::defaultValues()
    );

    auto prefilterImageArrayViews = Vec<vkrhi::VulkanImageView>::withCapacity(prefilterImageMips);
    for (u32 i = 0; i < prefilterImageMips; i++) {
        auto view = prefilterImage.createImageView(
            i, 1, 0, 6, false
        );
        prefilterImageArrayViews.emplace(std::move(view));
    }

    auto cb = vkrhi::VulkanCommandPoolsList::getAssignedGraphicsCommandPool().allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
    auto& cmd = cb.vkCommandBuffer();

    auto beginInfo = vk::CommandBufferBeginInfo{}
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    vkrhi::vkCheckResult(cmd.begin(beginInfo));

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(irradianceImage,
            vk::ImageLayout::eUndefined, {}, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(prefilterImage,
            vk::ImageLayout::eUndefined, {}, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(cb);

    // ---- Irradiance Image -----------------------------------------------------------------------------------------------------------------------------------

    auto imageAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(irradianceImageArrayView.vkImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto renderingArea = vk::Extent2D{irradianceImage.extent().width, irradianceImage.extent().height};

    auto viewport = vk::Viewport{}
        .setWidth(static_cast<f32>(renderingArea.width))
        .setHeight(static_cast<f32>(renderingArea.height))
        .setMinDepth(0.0)
        .setMaxDepth(1.0);
    auto scissor = vk::Rect2D{}.setExtent(renderingArea);

    auto renderingInfo = vk::RenderingInfo{}
        .setColorAttachments(imageAttachmentInfo)
        .setViewMask(0b111111)
        .setRenderArea(vk::Rect2D{}.setExtent(renderingArea));

    cmd.beginRendering(renderingInfo);
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_iblIrradianceCubeGeneratorPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(cb, m_iblIrradianceCubeGeneratorLayout, vk::PipelineBindPoint::eGraphics);

    struct PushConstants {
        u32 envmapTextureIndex;
        u32 envmapSamplerIndex;
        float cubeResolution;
    };

    auto pc = PushConstants {
        .envmapTextureIndex = TextureManager::get().textureToShaderIndexTable().textureToShaderImageIndex(m_skyCubemap.index),
        .envmapSamplerIndex = sampler,
        .cubeResolution = cubeResolution
    };

    cmd.pushConstants<PushConstants>(m_iblIrradianceCubeGeneratorLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pc);
    cmd.draw(3, 1, 0, 0);

    cmd.endRendering();

    // ---- Prefiltered Spec Maps ------------------------------------------------------------------------------------------------------------------------------

    for (u32 mip = 0; mip < prefilterImageMips; mip++) {
        auto imageAttachmentInfo = vk::RenderingAttachmentInfo{}
            .setImageView(prefilterImageArrayViews[mip].vkImageView())
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStoreOp(vk::AttachmentStoreOp::eStore);

        auto renderingArea = vk::Extent2D{std::max(1u, prefilterImage.extent().width >> mip), std::max(1u, prefilterImage.extent().height >> mip)};

        auto viewport = vk::Viewport{}
            .setWidth(static_cast<f32>(renderingArea.width))
            .setHeight(static_cast<f32>(renderingArea.height))
            .setMinDepth(0.0)
            .setMaxDepth(1.0);
        auto scissor = vk::Rect2D{}.setExtent(renderingArea);

        auto renderingInfo = vk::RenderingInfo{}
            .setColorAttachments(imageAttachmentInfo)
            .setViewMask(0b111111)
            .setRenderArea(vk::Rect2D{}.setExtent(renderingArea));

        cmd.beginRendering(renderingInfo);
        cmd.setViewport(0, viewport);
        cmd.setScissor(0, scissor);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_iblPrefilterCubeGeneratorPipeline.vkPipeline());
        TextureManager::get().shaderResourceTable().bindToCommandBuffer(cb, m_iblPrefilterCubeGeneratorLayout, vk::PipelineBindPoint::eGraphics);

        struct PushConstants {
            u32 envmapTextureIndex;
            u32 envmapSamplerIndex;
            float roughness;
            float cubeResolution;
        };

        auto pc = PushConstants {
            .envmapTextureIndex = TextureManager::get().textureToShaderIndexTable().textureToShaderImageIndex(m_skyCubemap.index),
            .envmapSamplerIndex = sampler,
            .roughness = prefilterImageMips > 1 ? static_cast<f32>(mip) / (static_cast<f32>(prefilterImageMips) - 1.0f) : 0.0f,
            .cubeResolution = cubeResolution
        };

        log::trace("Prefilter Spec Map Mip {} Roughness: {:.2f}", mip, pc.roughness);

        cmd.pushConstants<PushConstants>(m_iblPrefilterCubeGeneratorLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pc);
        cmd.draw(3, 1, 0, 0);

        cmd.endRendering();
    }

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(irradianceImage,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(prefilterImage,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .flush(cb);

    vkrhi::vkCheckResult(cmd.end());

    auto timeBefore = std::chrono::steady_clock::now();
    vkrhi::VulkanContext::get().vkQueueGraphics().submitOneCommandBuffer(cmd, {}, {}, None)
        .await();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - timeBefore);
    log::trace("Irradiance and Prefiltered Spec Maps built in {:.2f}ms", time.count() / 1000.0f);
}

} // namespace projnekomata