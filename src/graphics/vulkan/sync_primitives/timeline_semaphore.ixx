export module projnekomata:graphics.vulkan.sync_primitives.timeline_semaphore;
import vulkan;
import projnekomata.cs;
import :graphics.vulkan.vk_gpu_obrm;

export namespace projnekomata::gfx::vkrhi {

class VulkanTimelineSemaphore {
public:
    VulkanTimelineSemaphore(std::nullptr_t);
    VulkanTimelineSemaphore(vk::raii::Semaphore&& vkSemaphore);

    static auto create(u64 initialValue) -> VulkanTimelineSemaphore;

    VulkanTimelineSemaphore(const VulkanTimelineSemaphore&) = delete;
    VulkanTimelineSemaphore(VulkanTimelineSemaphore&&) = default;
    VulkanTimelineSemaphore& operator=(const VulkanTimelineSemaphore&) = delete;
    VulkanTimelineSemaphore& operator=(VulkanTimelineSemaphore&&) = default;

    [[nodiscard]] auto vkSemaphore() const -> const vk::raii::Semaphore& { return m_vkSemaphore.vkHandle(); }

private:
    VulkanAsyncRaiiWrapper<vk::raii::Semaphore> m_vkSemaphore = nullptr;
};

}