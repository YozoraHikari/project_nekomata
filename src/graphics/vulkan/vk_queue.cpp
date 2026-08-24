module projnekomata;
import std;
import vulkan;
import :graphics.vulkan.context;

namespace projnekomata::gfx::vkrhi {

auto GPUFuture::await(u64 timeout) const -> void {
    auto semWaitInfo = vk::SemaphoreWaitInfo{}
        .setSemaphores(*m_timelineSemaphore.get())
        .setValues(m_timelineSemaphoreTargetValue);

    VulkanContext::get().vkDevice().waitSemaphores(semWaitInfo, timeout);
}

VulkanQueue::VulkanQueue(std::nullptr_t) {}

VulkanQueue::VulkanQueue(vk::raii::Queue vkQueue, vk::raii::Semaphore vkTimelineSemaphore, u64 lastTimelineSubmissionValue)
    : m_vkQueue(std::move(std::move(vkQueue))), m_timelineSemaphore(std::move(vkTimelineSemaphore)), m_lastTimelineSubmissionValue(lastTimelineSubmissionValue) {}

VulkanQueue::~VulkanQueue() {
    m_vkQueue.waitIdle();
}

// TODO: Timeline semaphore support
auto VulkanQueue::submitOneCommandBuffer(const vk::raii::CommandBuffer& buf, Slice<const GPUFuture> asyncWaits,
                                            Slice<const vk::PipelineStageFlags2> asyncWaitStages,
                                            const Option<std::reference_wrapper<VulkanFence>>& fence) -> GPUFuture {
    std::scoped_lock lock(m_queueMutex);
    auto signalValue = m_lastTimelineSubmissionValue.fetch_add(1, std::memory_order_relaxed) + 1;

    auto semaphoreWaitInfos = Vec<vk::SemaphoreSubmitInfo>::withCapacity(asyncWaits.size());

    for (auto [op, flags] : asyncWaits.iter().zip(asyncWaitStages.iter())) {
        auto semaphoreSubmit = vk::SemaphoreSubmitInfo{}
            .setSemaphore(op.timelineSemaphore())
            .setValue(op.timelineSemaphoreTargetValue())
            .setStageMask(flags);

        semaphoreWaitInfos.emplace(semaphoreSubmit);
    }

    auto semaphoreSubmitInfo = vk::SemaphoreSubmitInfo{}
        .setSemaphore(m_timelineSemaphore)
        .setValue(signalValue)
        .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    auto commandBuffer = vk::CommandBufferSubmitInfo{}.setCommandBuffer(buf);

    auto submitInfo =
        vk::SubmitInfo2{}.setCommandBufferInfos(commandBuffer).setSignalSemaphoreInfos(semaphoreSubmitInfo).setWaitSemaphoreInfos(semaphoreWaitInfos);

    vkCheckResult(
        m_vkQueue.submit2(
            submitInfo,
            fence.map([](const auto& x) { return *x.get().vkFence(); })
                .unwrapOr(nullptr)
        )
    );

    auto asyncOp = GPUFuture(m_timelineSemaphore, signalValue);
    return asyncOp;
}

auto VulkanQueue::submitOneCommandBufferWithBinarySemaphores(const vk::raii::CommandBuffer& buf, Slice<const GPUFuture> asyncWaits,
                                                       Slice<const vk::PipelineStageFlags2> asyncWaitStages, const VulkanBinarySemaphore& binaryWait,
                                                       const VulkanBinarySemaphore& binarySignal, const vk::PipelineStageFlags2& binaryWaitStage,
                                                       const vk::PipelineStageFlags2& binarySignalStage,
                                                       const Option<std::reference_wrapper<VulkanFence>>& fence) -> GPUFuture {
    std::scoped_lock lock(m_queueMutex);
    auto signalValue = m_lastTimelineSubmissionValue.fetch_add(1, std::memory_order_relaxed) + 1;

    auto semaphoreWaitInfos = Vec<vk::SemaphoreSubmitInfo>::withCapacity(asyncWaits.size() + 1);

    for (auto [op, flags] : asyncWaits.iter().zip(asyncWaitStages.iter())) {
        auto semaphoreSubmit = vk::SemaphoreSubmitInfo{}
            .setSemaphore(op.timelineSemaphore())
            .setValue(op.timelineSemaphoreTargetValue())
            .setStageMask(flags);

        semaphoreWaitInfos.emplace(semaphoreSubmit);
    }

    auto binarySemaphoreWaitInfo = vk::SemaphoreSubmitInfo{}
        .setSemaphore(binaryWait.vkSemaphore())
        .setStageMask(binaryWaitStage);

    semaphoreWaitInfos.emplace(binarySemaphoreWaitInfo);

    auto semaphoreSignalInfos = Vec<vk::SemaphoreSubmitInfo>::withCapacity(2);
    auto semaphoreSubmitInfo = vk::SemaphoreSubmitInfo{}
        .setSemaphore(m_timelineSemaphore)
        .setValue(signalValue)
        .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
    
    semaphoreSignalInfos.emplace(semaphoreSubmitInfo);

    auto binarySemaphoreSignalInfo = vk::SemaphoreSubmitInfo{}
        .setSemaphore(binarySignal.vkSemaphore())
        .setStageMask(binarySignalStage);
    semaphoreSignalInfos.emplace(binarySemaphoreSignalInfo);

    auto commandBuffer = vk::CommandBufferSubmitInfo{}.setCommandBuffer(buf);

    auto submitInfo =
        vk::SubmitInfo2{}.setCommandBufferInfos(commandBuffer).setSignalSemaphoreInfos(semaphoreSignalInfos).setWaitSemaphoreInfos(semaphoreWaitInfos);

    vkCheckResult(
        m_vkQueue.submit2(
            submitInfo,
            fence.map([](const auto& x) { return *x.get().vkFence(); })
                .unwrapOr(nullptr)
        )
    );

    auto asyncOp = GPUFuture(m_timelineSemaphore, signalValue);
    return asyncOp;
}

auto VulkanQueue::submitPresent(const VulkanSwapchain& swapchain, const VulkanBinarySemaphore& waitSemaphore, u32 imageIndex) -> vk::Result {
    std::scoped_lock lock(m_queueMutex);

    auto presentInfo = vk::PresentInfoKHR{}
        .setWaitSemaphores(*waitSemaphore.vkSemaphore())
        .setImageIndices(imageIndex)
        .setSwapchains(*swapchain.vkSwapchain());
    
    return m_vkQueue.presentKHR(presentInfo);
}

auto VulkanQueue::waitIdle() -> void {
    vkCheckResult(m_vkQueue.waitIdle());
}
auto VulkanQueue::currentTimelineValue() const -> u64 {
    return vkCheckResult(m_timelineSemaphore.getCounterValue());
}

} // namespace projnekomata::gfx::vkrhi