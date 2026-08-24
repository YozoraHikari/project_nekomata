module projnekomata;
import :graphics.vulkan.vk_pipeline_layout;

namespace projnekomata::gfx::vkrhi {

VulkanPipelineLayout::VulkanPipelineLayout(std::nullptr_t) : m_vkPipelineLayout(nullptr) {}
VulkanPipelineLayout::VulkanPipelineLayout(vk::raii::PipelineLayout&& vkPipelineLayout) : m_vkPipelineLayout(std::move(vkPipelineLayout)) {}

auto VulkanPipelineLayout::builder() -> VulkanPipelineLayoutBuilder {
    return {};
}

}