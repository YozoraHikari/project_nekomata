export module projnekomata:graphics.vulkan.sync_primitives.fence;
import vulkan;
import projnekomata.cs;
import :graphics.vulkan.vk_gpu_obrm;

export namespace projnekomata::gfx::vkrhi {

class VulkanFence {
public:
    VulkanFence(std::nullptr_t);
    VulkanFence(vk::raii::Fence&& vkFence);

    static auto create(bool signaled) -> VulkanFence;

    VulkanFence(const VulkanFence&) = delete;
    VulkanFence(VulkanFence&&) = default;
    VulkanFence& operator=(const VulkanFence&) = delete;
    VulkanFence& operator=(VulkanFence&&) = default;

    [[nodiscard]] auto vkFence() const -> const vk::raii::Fence& { return m_vkFence.vkHandle(); }

    auto waitForSignal(u64 timeoutNanos) const -> void;
    auto reset() const -> void;

private:
    VulkanAsyncRaiiWrapper<vk::raii::Fence> m_vkFence = nullptr;

};

}