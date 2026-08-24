module projnekomata;
import :graphics.vulkan.context;
import :graphics.vulkan.sync_primitives.timeline_semaphore;

namespace projnekomata::gfx::vkrhi {

VulkanTimelineSemaphore::VulkanTimelineSemaphore(std::nullptr_t) {}
VulkanTimelineSemaphore::VulkanTimelineSemaphore(vk::raii::Semaphore&& vkSemaphore) : m_vkSemaphore(std::move(vkSemaphore)) {}

auto VulkanTimelineSemaphore::create(u64 initialValue) -> VulkanTimelineSemaphore {
    auto semaphoreTypeInfo = vk::SemaphoreTypeCreateInfo{}
        .setSemaphoreType(vk::SemaphoreType::eTimeline)
        .setInitialValue(initialValue);
    
    auto chain = vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo>{
        vk::SemaphoreCreateInfo{},
        semaphoreTypeInfo
    };

    auto handle = vkCheckResult(VulkanContext::get().vkDevice().createSemaphore(chain.get<vk::SemaphoreCreateInfo>()));
    return VulkanTimelineSemaphore(std::move(handle));
}

}