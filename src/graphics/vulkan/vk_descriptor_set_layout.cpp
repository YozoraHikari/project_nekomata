module projnekomata;
import :graphics.vulkan.vk_descriptor_set_layout;

namespace projnekomata::gfx::vkrhi {

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(std::nullptr_t) {  }
VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(vk::raii::DescriptorSetLayout&& vkDescriptorSetLayout) : m_vkDescriptorSetLayout(std::move(vkDescriptorSetLayout)) {}

auto VulkanDescriptorSetLayout::builder() -> VulkanDescriptorSetLayoutBuilder {
    return {};
}

} // namespace projnekomata::gfx::vkrhi