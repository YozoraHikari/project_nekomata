export module projnekomata:graphics.rendering.frame_context;
import std;
import :graphics.rendering.transient_rendering_resources;
import :graphics.rendering.shared_rendering_resources;
import :graphics.vulkan.vk_swapchain;
import :core.runtime.shared_data;
import :graphics.rendering.frame_rendering_resources;

export namespace projnekomata::gfx {

struct FrameResult {
    bool shouldRecreateSwapchain = false;
    bool stepPerFrameResources = false;
};

enum class FrameContextTimestampIndex {
    BeforeGeometryPass,
    AfterGeometryPass,
    BeforeLightingPass,
    AfterLightingPass,
    BeforeBloomFilter,
    AfterBloomFilter,
    BeforeTonemapFuse,
    AfterTonemapFuse,
    BeforeSmaa,
    AfterSmaa,
    BeforeUI,
    AfterUI,
    CountDiscrim
};

class FrameContext {
public:
    FrameContext(std::nullptr_t);
    FrameContext();

    auto waitForLastFrame() -> void;

    [[nodiscard]] auto execute(TransientRenderingResources& transientRenderingResources, SharedRenderingResources& sharedRenderingResources,
        vkrhi::VulkanSwapchain& swapchain, MRThreadsSharedDataLeaf& renderingData, MRThreadsSharedData& threadSharedData, bool recordStatistics) -> FrameResult;

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------
    // Statistics

    vkrhi::VulkanQueryPool m_timestampsQueryPool = nullptr;
    vkrhi::VulkanQueryPool m_pipelineStatisticsQueryPool = nullptr;
    bool m_queryPoolsHaveResultsOnFinish = false;
    u64 m_numDrawcalls = 0;

private:
    FrameRenderingResources m_frameRenderingResources = nullptr;
};

}