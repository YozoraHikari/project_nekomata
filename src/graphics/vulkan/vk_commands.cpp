module projnekomata;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_commands;

namespace projnekomata::gfx::vkrhi {

VulkanCommandBuffer::VulkanCommandBuffer(std::nullptr_t) {}
VulkanCommandBuffer::VulkanCommandBuffer(vk::raii::CommandBuffer&& vkCommandBuffer) : m_vkCommandBuffer(std::move(vkCommandBuffer)) {}

VulkanCommandPool::VulkanCommandPool(std::nullptr_t) {}
VulkanCommandPool::VulkanCommandPool(vk::raii::CommandPool&& vkCommandPool) : m_vkCommandPool(std::move(vkCommandPool)) {}

auto VulkanCommandPool::createForGraphics(bool transientCommandBuffers) -> VulkanCommandPool {
    auto queueFamilyIndex = VulkanContext::get().vkPhysicalDeviceProps().m_graphicsQueueIndex;
    return createWithQueueFamilyIndex(queueFamilyIndex, transientCommandBuffers);
}

auto VulkanCommandPool::createForAsyncCompute(bool transientCommandBuffers) -> VulkanCommandPool {
    auto queueFamilyIndex = VulkanContext::get().vkPhysicalDeviceProps().m_asyncComputeQueueIndex;
    return createWithQueueFamilyIndex(queueFamilyIndex, transientCommandBuffers);
}

auto VulkanCommandPool::createWithQueueFamilyIndex(u32 queueFamilyIndex, bool transientCommandBuffers) -> VulkanCommandPool {
    auto createInfo = vk::CommandPoolCreateInfo{}
        .setQueueFamilyIndex(queueFamilyIndex);
    
    if (transientCommandBuffers) createInfo.flags |= vk::CommandPoolCreateFlagBits::eTransient;

    auto handle = vkCheckResult(VulkanContext::get().vkDevice().createCommandPool(createInfo));
    return VulkanCommandPool(std::move(handle));
}

auto VulkanCommandPool::reset() -> void {
    m_vkCommandPool.vkHandle().reset();
}

auto VulkanCommandPool::allocateCommandBuffer(vk::CommandBufferLevel level) -> VulkanCommandBuffer {
    auto allocInfo = vk::CommandBufferAllocateInfo{}
        .setCommandPool(m_vkCommandPool.vkHandle())
        .setCommandBufferCount(1)
        .setLevel(level);
    
    auto buffers = vkCheckResult(VulkanContext::get().vkDevice().allocateCommandBuffers(allocInfo));
    return VulkanCommandBuffer(std::move(buffers[0]));
}

}
