export module projnekomata:graphics.vulkan.sync_primitives.binary_semaphore;
import vulkan;
import :graphics.vulkan.vk_gpu_obrm;

export namespace projnekomata::gfx::vkrhi {

class VulkanBinarySemaphore {
public:
    VulkanBinarySemaphore(std::nullptr_t);
    VulkanBinarySemaphore(vk::raii::Semaphore&& vkSemaphore);

    static auto create() -> VulkanBinarySemaphore;

    VulkanBinarySemaphore(const VulkanBinarySemaphore&) = delete;
    VulkanBinarySemaphore(VulkanBinarySemaphore&&) = default;
    VulkanBinarySemaphore& operator=(const VulkanBinarySemaphore&) = delete;
    VulkanBinarySemaphore& operator=(VulkanBinarySemaphore&&) = default;

    [[nodiscard]] auto vkSemaphore() const -> const vk::raii::Semaphore& { return m_vkSemaphore.vkHandle(); }

private:
    VulkanAsyncRaiiWrapper<vk::raii::Semaphore> m_vkSemaphore = nullptr;
};

}