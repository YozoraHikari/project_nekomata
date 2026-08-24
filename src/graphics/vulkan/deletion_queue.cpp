module projnekomata;
import projnekomata.corelib;
import :graphics.vulkan.context;

namespace projnekomata::gfx::vkrhi {

VulkanResourceDeletionQueue::~VulkanResourceDeletionQueue() {
    VulkanContext::get().vkQueueGraphics().waitIdle();
    VulkanContext::get().vkQueueAsyncCompute().waitIdle();

    auto& graphicsQueueSemaphore = VulkanContext::get().vkQueueGraphics().timelineSemaphore();
    auto& asyncComputeQueueSemaphore = VulkanContext::get().vkQueueAsyncCompute().timelineSemaphore();

    m_shouldRun.store(false, std::memory_order_release);
    m_queueCv.notify_all();

    auto signalInfo = vk::SemaphoreSignalInfo{}
        .setSemaphore(graphicsQueueSemaphore)
        .setValue(std::numeric_limits<u64>::max());

    vkCheckResult(VulkanContext::get().vkDevice().signalSemaphore(signalInfo));

    signalInfo.semaphore = asyncComputeQueueSemaphore;
    vkCheckResult(VulkanContext::get().vkDevice().signalSemaphore(signalInfo));


    m_endSyncBarrier.arrive_and_wait();
    m_workerThread.join();
}

auto VulkanResourceDeletionQueue::run() -> void {
    m_workerThread = std::thread(&VulkanResourceDeletionQueue::workerRoutine, this);
}

auto VulkanResourceDeletionQueue::pushObject(const GpuResourceRetireTimelineValues& marker, AnyVulkanObject&& obj) -> void {
    ResourceDeletionQueueEntry val {
        .m_graphicsQueueRetireValue = marker.m_graphicsQueueRetireValue,
        .m_asyncComputeQueueRetireValue = marker.m_asyncComputeQueueRetireValue,
        .m_vkObject = std::move(obj)
    };
    m_objectsMpscQueue.push(std::move(val));

    std::lock_guard lk(m_cvLock);
    m_queueCv.notify_one();
}

auto drop(AnyVulkanObject&& _) -> void {}

auto VulkanResourceDeletionQueue::workerRoutine() -> void {
    Thread::setThreadName("VulkanOBRM");

    u64 currentGraphicsQueueRetireValue = 0;
    u64 currentAsyncComputeQueueRetireValue = 0;

    auto& graphicsQueueSemaphore = VulkanContext::get().vkQueueGraphics().timelineSemaphore();
    auto& asyncComputeQueueSemaphore = VulkanContext::get().vkQueueAsyncCompute().timelineSemaphore();
    auto semaphores = std::array<vk::Semaphore, 2>{graphicsQueueSemaphore, asyncComputeQueueSemaphore};

    while (m_shouldRun.load(std::memory_order_acquire)) {
        while (auto objOpt = m_objectsMpscQueue.pop()) {
            auto obj = std::move(objOpt.unwrap());
            if (!(obj.m_graphicsQueueRetireValue <= currentGraphicsQueueRetireValue && obj.m_asyncComputeQueueRetireValue <= currentAsyncComputeQueueRetireValue)) {
                auto values = std::array<u64, 2>{obj.m_graphicsQueueRetireValue, obj.m_asyncComputeQueueRetireValue};

                auto waitInfo = vk::SemaphoreWaitInfo{}
                    .setSemaphores(semaphores)
                    .setValues(values);

                // log::info(" ** Vulkan OBRM thread waiting:    current graphics = {}, async compute = {}, expected graphics = {}, async compute = {}", current_graphics_queue_retire_value, current_async_compute_queue_retire_value, obj->graphics_queue_retire_value, obj->async_compute_queue_retire_value);
                vkCheckResult(VulkanContext::get().vkDevice().waitSemaphores(waitInfo, std::numeric_limits<u64>::max()));


                currentGraphicsQueueRetireValue = vkCheckResult(graphicsQueueSemaphore.getCounterValue());
                currentAsyncComputeQueueRetireValue = vkCheckResult(asyncComputeQueueSemaphore.getCounterValue());
                // log::info(" ** Vulkan OBRM thread continuing:     new graphics = {}, async compute = {}", current_graphics_queue_retire_value, current_async_compute_queue_retire_value);
            }

            // log::info("Vulkan OBRM: Vulkan object retired with graphics = {}, async compute = {}", obj->graphics_queue_retire_value, obj->async_compute_queue_retire_value);

            drop(std::move(obj.m_vkObject));
        }

        std::unique_lock<std::mutex> lock(m_cvLock);
        m_queueCv.wait(lock, [&]{ return m_objectsMpscQueue.hasElement() || !m_shouldRun.load(std::memory_order_acquire); });
    }

    m_endSyncBarrier.arrive_and_wait();
}

auto VulkanResourceDeletionQueue::get() -> VulkanResourceDeletionQueue& {
    return *g_vkResourceDeletionQueue;
}

}