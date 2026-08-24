export module projnekomata:graphics.vulkan.vk_gpu_obrm_structs;
import projnekomata.corelib;

export namespace projnekomata::gfx::vkrhi {

struct GpuResourceRetireTimelineValues {
    u64 m_graphicsQueueRetireValue = 0;
    u64 m_asyncComputeQueueRetireValue = 0;

    constexpr auto hasAdvancedAfter(GpuResourceRetireTimelineValues& val) const -> bool {
        return (m_graphicsQueueRetireValue >= val.m_graphicsQueueRetireValue)
            && (m_asyncComputeQueueRetireValue >= val.m_asyncComputeQueueRetireValue);
    }

    static auto latestSubmitValues() -> GpuResourceRetireTimelineValues;
    static auto queueCurrentValues() -> GpuResourceRetireTimelineValues;
};

} // namespace projnekomata::gfx::vkrhi