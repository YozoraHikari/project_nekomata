module projnekomata;
import :graphics.vulkan.context;

namespace projnekomata::gfx::vkrhi {

VulkanBinarySemaphore::VulkanBinarySemaphore(std::nullptr_t) {}
VulkanBinarySemaphore::VulkanBinarySemaphore(vk::raii::Semaphore&& vkSemaphore) : m_vkSemaphore(std::move(vkSemaphore)) {}

auto VulkanBinarySemaphore::create() -> VulkanBinarySemaphore {
    auto semaphoreCreateInfo = vk::SemaphoreCreateInfo{};

    auto handle = vkCheckResult(VulkanContext::get().vkDevice().createSemaphore(semaphoreCreateInfo));
    return VulkanBinarySemaphore(std::move(handle));
}

}