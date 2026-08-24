module projnekomata;
import :graphics.vulkan.vk_descriptor_set;

namespace projnekomata::gfx::vkrhi {

VulkanDescriptorSet::VulkanDescriptorSet(std::nullptr_t) {  }
VulkanDescriptorSet::VulkanDescriptorSet(vk::raii::DescriptorSet&& vkDescriptorSet)
    : m_vkDescriptorSet(std::move(vkDescriptorSet)) {}

VulkanDescriptorSet::~VulkanDescriptorSet() {
    if (!m_isFromFreeDescriptorSetPool) {
        m_vkDescriptorSet = nullptr;
    }
}

} // namespace projnekomata::gfx::vkrhi