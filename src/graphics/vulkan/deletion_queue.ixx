export module projnekomata:graphics.vulkan.deletion_queue;
import std;
import projnekomata.cs;
import vulkan;
import vk_mem_alloc;
import :core.containers.mpsc_queue;
import :graphics.vulkan.vk_gpu_obrm_structs;

export namespace projnekomata::gfx::vkrhi {

using AnyVulkanObject = FlatVariant<vk::raii::CommandPool, vk::raii::CommandBuffer, vk::raii::Image, vk::raii::ImageView, vk::raii::SwapchainKHR, vk::raii::Semaphore,
                vk::raii::Fence, vma::raii::Allocation, vk::raii::PipelineLayout, vk::raii::Pipeline, vk::raii::Buffer, vma::raii::VirtualBlock,
                vk::raii::DescriptorSetLayout, vk::raii::DescriptorSet, vk::raii::DescriptorPool, vk::raii::Sampler, vk::raii::QueryPool>;

struct ResourceDeletionQueueEntry {
    u64 m_graphicsQueueRetireValue;
    u64 m_asyncComputeQueueRetireValue;
    AnyVulkanObject m_vkObject;
};

class VulkanResourceDeletionQueue {
public:
    ~VulkanResourceDeletionQueue();
    auto run() -> void;
    static auto get() -> VulkanResourceDeletionQueue&;
    auto pushObject(const GpuResourceRetireTimelineValues& marker, AnyVulkanObject&& obj) -> void;

private:
    auto workerRoutine() -> void;

    std::thread m_workerThread;
    std::barrier<> m_endSyncBarrier = std::barrier(2);
    std::atomic<bool> m_shouldRun = std::atomic(true);
    AtomicMpscQueue<ResourceDeletionQueueEntry> m_objectsMpscQueue;

    std::mutex m_cvLock;
    std::condition_variable m_queueCv;
};

inline VulkanResourceDeletionQueue* g_vkResourceDeletionQueue = nullptr;

}