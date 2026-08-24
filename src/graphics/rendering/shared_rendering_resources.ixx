export module projnekomata:graphics.rendering.shared_rendering_resources;
import projnekomata.corelib;
import :graphics.meshsystem.mesh_asset_storage;
import :graphics.vulkan.vk_pipeline_layout;
import :graphics.vulkan.vk_pipeline_graphics;
import :graphics.fontsystem.font_face;
import :graphics.fontsystem.dynamic_font_atlas;
import :graphics.materialsystem.mat_manager;
import :graphics.vulkan.vk_descriptor_pool;

export namespace projnekomata::gfx {

struct MeshHysteresisState {
    u32 currentLod = kLodListMaxLodCount - 1;
};

/// Shared rendering resources house data used across all render steps.
///
/// # Access Contingency
/// | CPU Read/Write | GPU Access Scope across Queue | GPU Visibility Scope across Queue |
/// |----------------|-------------------------------|-----------------------------------|
/// | Yes            | Shared                        | Shared                            |
///
class SharedRenderingResources {
public:
    SharedRenderingResources(std::nullptr_t);
    SharedRenderingResources();

    auto checkGraphicsSettingsAndMaybeRecompileShaders() -> void;
    auto refitHysteresisStates(usize renderableSparseCount) -> void;
    auto getHysteresisState(usize renderableSparseIndex) -> MeshHysteresisState& { return m_meshHysteresisStates[renderableSparseIndex]; }
    auto getLastRenderableModelMatrix(usize renderableSparseIndex) -> math::Matrix4x4f& { return m_lastRenderableModelMatrices[renderableSparseIndex]; }

    bool smaaShaderNeedsRecompile = false;

    float displayMs = 0.0f;

    Texture m_skyCubemap = {};
    Texture m_skyIrradianceCubemap = {};
    Texture m_skyPrefilterCubemap = {};
    Texture m_brdfLUT = {};

    Texture m_smaaAreaTexture = {};
    Texture m_smaaSearchTexture = {};

    vkrhi::VulkanPipelineLayout m_iblIrradianceCubeGeneratorLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_iblIrradianceCubeGeneratorPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_iblPrefilterCubeGeneratorLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_iblPrefilterCubeGeneratorPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_bitmapFontRendererLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_bitmapFontRendererPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_uiRectRendererLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_uiRectRendererPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_uiTextureRendererLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_uiTextureRendererPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_mainLightingPassLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_mainLightingPassPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_smaaBlendWeightLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_smaaBlendWeightPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_smaaEdgeDetectLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_smaaEdgeDetectPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_smaaNeighborhoodBlendLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_smaaNeighborhoodBlendPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_smaaTemporalResolveLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_smaaTemporalResolvePipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_velbufferBgLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_velbufferBgPipeline = nullptr;

    vkrhi::VulkanPipelineLayout m_quadOverdrawVisLayout = nullptr;
    vkrhi::VulkanGraphicsPipeline m_quadOverdrawVisPipeline = nullptr;

    math::Matrix4x4f m_lastProjview = math::Matrix4x4f::identity();
    math::Matrix4x4f m_lastProjviewNoTranslation = math::Matrix4x4f::identity();

private:
    u32 m_compiledSmaaPreset = 3;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Hysteresis State
    Vec<MeshHysteresisState> m_meshHysteresisStates = Vec<MeshHysteresisState>::create();
    Vec<math::Matrix4x4f> m_lastRenderableModelMatrices = Vec<math::Matrix4x4f>::create();

    auto buildSmaaPipelines() -> void;
    auto buildIblSecondaryCubemaps() -> void;
};

} // namespace projnekomata