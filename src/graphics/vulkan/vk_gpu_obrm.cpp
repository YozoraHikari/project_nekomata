module projnekomata;
import :graphics.vulkan.vk_gpu_obrm;

namespace projnekomata::gfx::vkrhi {
template class VulkanAsyncRaiiWrapper<vk::raii::CommandPool>;
template class VulkanAsyncRaiiWrapper<vk::raii::CommandBuffer>;
template class VulkanAsyncRaiiWrapper<vk::raii::Image>;
template class VulkanAsyncRaiiWrapper<vk::raii::ImageView>;
template class VulkanAsyncRaiiWrapper<vk::raii::SwapchainKHR>;
template class VulkanAsyncRaiiWrapper<vk::raii::Semaphore>;
template class VulkanAsyncRaiiWrapper<vk::raii::Fence>;
template class VulkanAsyncRaiiWrapper<vma::raii::Allocation>;
template class VulkanAsyncRaiiWrapper<vk::raii::PipelineLayout>;
template class VulkanAsyncRaiiWrapper<vk::raii::Pipeline>;
template class VulkanAsyncRaiiWrapper<vk::raii::Buffer>;
template class VulkanAsyncRaiiWrapper<vma::raii::VirtualBlock>;
template class VulkanAsyncRaiiWrapper<vk::raii::DescriptorSetLayout>;
template class VulkanAsyncRaiiWrapper<vk::raii::DescriptorSet>;
template class VulkanAsyncRaiiWrapper<vk::raii::DescriptorPool>;
template class VulkanAsyncRaiiWrapper<vk::raii::Sampler>;
template class VulkanAsyncRaiiWrapper<vk::raii::QueryPool>;
}