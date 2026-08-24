export module projnekomata:graphics.vulkan.vk_queue;
import std;
import vulkan;
import projnekomata.corelib;
import :graphics.vulkan.vk_swapchain;
import :graphics.vulkan.sync_primitives.fence;
import :graphics.vulkan.sync_primitives.binary_semaphore;

export namespace projnekomata::gfx::vkrhi {

class GPUFuture {
public:
    GPUFuture(const vk::raii::Semaphore& timelineSemaphore, u64 timelineSemaphoreTargetValue)
        : m_timelineSemaphore(timelineSemaphore), m_timelineSemaphoreTargetValue(timelineSemaphoreTargetValue) {}

    [[nodiscard]] auto timelineSemaphore() const -> const vk::raii::Semaphore& { return m_timelineSemaphore; }
    [[nodiscard]] auto timelineSemaphoreTargetValue() const -> u64 { return m_timelineSemaphoreTargetValue; }

    auto await(u64 timeout = std::numeric_limits<u64>::max()) const -> void;

private:
    std::reference_wrapper<const vk::raii::Semaphore> m_timelineSemaphore;
    u64 m_timelineSemaphoreTargetValue;
};

class VulkanQueue {
public:
    VulkanQueue(std::nullptr_t);
    VulkanQueue(vk::raii::Queue vkQueue, vk::raii::Semaphore vkTimelineSemaphore, u64 lastTimelineSubmissionValue);
    ~VulkanQueue();

    VulkanQueue(const VulkanQueue&) = delete;
    VulkanQueue(VulkanQueue&&) = delete;
    VulkanQueue& operator=(const VulkanQueue&) = delete;
    VulkanQueue& operator=(VulkanQueue&&) = delete;

    auto submitOneCommandBuffer(const vk::raii::CommandBuffer& buf, Slice<const GPUFuture> asyncWaits,
                                   Slice<const vk::PipelineStageFlags2> asyncWaitStages, const Option<std::reference_wrapper<VulkanFence>>& fence)
        -> GPUFuture;
    auto submitOneCommandBufferWithBinarySemaphores(const vk::raii::CommandBuffer& buf, Slice<const GPUFuture> asyncWaits,
                                              Slice<const vk::PipelineStageFlags2> asyncWaitStages, const VulkanBinarySemaphore& binaryWait,
                                              const VulkanBinarySemaphore& binarySignal, const vk::PipelineStageFlags2& binaryWaitStage,
                                              const vk::PipelineStageFlags2& binarySignalStage,
                                              const Option<std::reference_wrapper<VulkanFence>>& fence) -> GPUFuture;
    auto submitPresent(const VulkanSwapchain& swapchain, const VulkanBinarySemaphore& waitSemaphore, u32 imageIndex) -> vk::Result;
    auto waitIdle() -> void;

    auto timelineSemaphore() -> const vk::raii::Semaphore& { return m_timelineSemaphore; }
    auto lastTimelineSubmissionValue() const -> u64 {
        return m_lastTimelineSubmissionValue.load(std::memory_order_relaxed);
    }
    auto currentTimelineValue() const -> u64;

private:
    mutable std::mutex m_queueMutex;
    vk::raii::Queue m_vkQueue = nullptr;
    vk::raii::Semaphore m_timelineSemaphore = nullptr;
    std::atomic<u64> m_lastTimelineSubmissionValue = 0;
};

} // namespace projnekomata::gfx::vkrhi
