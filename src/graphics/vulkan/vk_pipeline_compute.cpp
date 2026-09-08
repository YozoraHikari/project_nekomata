module projnekomata;
import :graphics.vulkan.vk_pipeline_compute;

namespace projnekomata::gfx::vkrhi {

VulkanComputePipeline::VulkanComputePipeline(std::nullptr_t) {}
VulkanComputePipeline::VulkanComputePipeline(vk::raii::Pipeline&& vkPipeline) : m_vkPipeline(std::move(vkPipeline)) {}

auto VulkanComputePipeline::builder() -> VulkanComputePipelineBuilder {
    return {};
}

VulkanComputePipelineBuilder::VulkanComputePipelineBuilder() = default;
VulkanComputePipelineBuilder::VulkanComputePipelineBuilder(std::nullptr_t) {}

}