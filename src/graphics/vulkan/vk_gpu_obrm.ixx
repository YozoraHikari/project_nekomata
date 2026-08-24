export module projnekomata:graphics.vulkan.vk_gpu_obrm;
import std;
import vulkan;
import :graphics.vulkan.deletion_queue;
import :graphics.vulkan.vk_gpu_obrm_structs;

export namespace projnekomata::gfx::vkrhi {

template <typename T>
concept AnyVulkanRaiiHandle = requires {
    { vk::raii::isVulkanRAIIHandleType<T>::value } -> std::same_as<const bool&>;
    requires vk::raii::isVulkanRAIIHandleType<T>::value;
};

template <typename T>
class VulkanAsyncRaiiWrapper {
public:
    VulkanAsyncRaiiWrapper(std::nullptr_t) : m_vkHandle(nullptr) {}
    VulkanAsyncRaiiWrapper(T&& vkHandle) : m_vkHandle(std::move(vkHandle)) {}
    ~VulkanAsyncRaiiWrapper() {
        if (m_vkHandle != nullptr) {
            auto raiiUseMarker = GpuResourceRetireTimelineValues::latestSubmitValues();
            VulkanResourceDeletionQueue::get().pushObject(raiiUseMarker, std::move(m_vkHandle));
        }
    }

    VulkanAsyncRaiiWrapper(const VulkanAsyncRaiiWrapper&) = delete;
    VulkanAsyncRaiiWrapper(VulkanAsyncRaiiWrapper&&) = default;
    VulkanAsyncRaiiWrapper& operator=(const VulkanAsyncRaiiWrapper&) = delete;
    VulkanAsyncRaiiWrapper& operator=(VulkanAsyncRaiiWrapper&&) = default;

    [[nodiscard]] auto vkHandle() const -> const T& {
        return m_vkHandle;
    }

private:
    T m_vkHandle = nullptr;
};

extern template class VulkanAsyncRaiiWrapper<vk::raii::CommandPool>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::CommandBuffer>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::Image>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::ImageView>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::SwapchainKHR>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::Semaphore>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::Fence>;
extern template class VulkanAsyncRaiiWrapper<vma::raii::Allocation>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::PipelineLayout>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::Pipeline>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::Buffer>;
extern template class VulkanAsyncRaiiWrapper<vma::raii::VirtualBlock>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::DescriptorSetLayout>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::DescriptorSet>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::DescriptorPool>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::Sampler>;
extern template class VulkanAsyncRaiiWrapper<vk::raii::QueryPool>;


} // namespace projnekomata::gfx::vkrhi