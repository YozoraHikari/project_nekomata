module projnekomata;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_descriptor_pool;

namespace projnekomata::gfx::vkrhi {

VulkanDescriptorPool::VulkanDescriptorPool(std::nullptr_t) {  }
VulkanDescriptorPool::VulkanDescriptorPool(vk::raii::DescriptorPool&& vkDescriptorPool) : m_vkDescriptorPool(std::move(vkDescriptorPool)) {}

auto VulkanDescriptorPool::create(u32 maxSets, Slice<const vk::DescriptorPoolSize> sizes, bool updateAfterBindPool, bool freeDescriptorSetPool) -> VulkanDescriptorPool {
    auto flags = vk::DescriptorPoolCreateFlags{};
    if (updateAfterBindPool) flags |= vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    if (freeDescriptorSetPool) flags |= vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    auto descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo{}
        .setMaxSets(maxSets)
        .setPoolSizes(sizes)
        .setFlags(flags);

    auto handle = vkCheckResult(VulkanContext::get().vkDevice().createDescriptorPool(descriptorPoolCreateInfo));
    return VulkanDescriptorPool(std::move(handle));
}

auto VulkanDescriptorPool::builder() -> VulkanDescriptorPoolBuilder {
    return {};
}

auto VulkanDescriptorPool::reset() -> void {
    m_vkDescriptorPool.vkHandle().reset();
}

auto VulkanDescriptorPool::allocateDescriptorSet(const VulkanDescriptorSetLayout& descriptorSetLayout) -> VulkanDescriptorSet {
    auto descriptorSetAllocInfo = vk::DescriptorSetAllocateInfo{}
        .setDescriptorSetCount(1)
        .setDescriptorPool(m_vkDescriptorPool.vkHandle())
        .setSetLayouts(*descriptorSetLayout.vkDescriptorSetLayout());

    auto descriptorSet = std::move(vkCheckResult(VulkanContext::get().vkDevice().allocateDescriptorSets(descriptorSetAllocInfo))[0]);
    return VulkanDescriptorSet(std::move(descriptorSet));
}

} // namespace projnekomata::gfx::vkrhi