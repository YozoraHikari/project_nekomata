module projnekomata;
import :graphics.vulkan.vk_query_pool;

namespace projnekomata::gfx::vkrhi {

VulkanQueryPool::VulkanQueryPool(std::nullptr_t) {}
VulkanQueryPool::VulkanQueryPool(vk::raii::QueryPool&& vkQueryPool, u32 queryCount) : m_vkQueryPool(std::move(vkQueryPool)), m_queryCount(queryCount) {}

auto VulkanQueryPool::create(vk::QueryType qtype, u32 qcount, vk::QueryPipelineStatisticFlags pipelineStatisticFlags) -> VulkanQueryPool {
    auto createInfo = vk::QueryPoolCreateInfo{}
        .setQueryType(qtype)
        .setQueryCount(qcount)
        .setPipelineStatistics(pipelineStatisticFlags);

    auto handle = vkCheckResult(VulkanContext::get().vkDevice().createQueryPool(createInfo));
    return VulkanQueryPool(std::move(handle), qcount);
}

} // namespace projnekomata::gfx::vkrhi
