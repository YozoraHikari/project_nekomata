export module projnekomata:graphics.vulkan.vk_query_pool;
import std;
import vulkan;
import projnekomata.corelib;
import :graphics.vulkan.vk_gpu_obrm;

export namespace projnekomata::gfx::vkrhi {

class VulkanQueryPool {
public:
    VulkanQueryPool(std::nullptr_t);
    VulkanQueryPool(vk::raii::QueryPool&& vkQueryPool, u32 queryCount);

    static auto create(vk::QueryType qtype, u32 qcount, vk::QueryPipelineStatisticFlags pipelineStatisticFlags) -> VulkanQueryPool;

    VulkanQueryPool(const VulkanQueryPool&) = delete;
    VulkanQueryPool(VulkanQueryPool&&) = default;
    VulkanQueryPool& operator=(const VulkanQueryPool&) = delete;
    VulkanQueryPool& operator=(VulkanQueryPool&&) = default;

    [[nodiscard]] auto vkQueryPool() const -> const vk::raii::QueryPool& { return m_vkQueryPool.vkHandle(); }
    [[nodiscard]] auto queryCount() const -> u32 { return m_queryCount; }

private:
    VulkanAsyncRaiiWrapper<vk::raii::QueryPool> m_vkQueryPool = nullptr;
    u32 m_queryCount;
};

}