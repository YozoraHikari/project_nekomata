export module projnekomata:graphics.vulkan.vk_sampler;
import vulkan;
import projnekomata.corelib;
import :graphics.vulkan.vk_gpu_obrm;

export namespace projnekomata::gfx::vkrhi {

class VulkanSampler {
public:
    VulkanSampler(std::nullptr_t);
    VulkanSampler(vk::raii::Sampler&& vkSampler);

    static auto create(vk::Filter minFilter, vk::Filter magFilter, vk::SamplerMipmapMode mipmapMode, vk::SamplerAddressMode addressModeU, vk::SamplerAddressMode addressModeV, vk::SamplerAddressMode addressModeW, bool anisotropyEnable, f32 anisotropy, f32 minLod, f32 maxLod, f32 mipLodBias) -> VulkanSampler;

    VulkanSampler(const VulkanSampler&) = delete;
    VulkanSampler(VulkanSampler&&) = default;
    VulkanSampler& operator=(const VulkanSampler&) = delete;
    VulkanSampler& operator=(VulkanSampler&&) = default;

    [[nodiscard]] auto vkSampler() const -> const vk::raii::Sampler& { return m_vkSampler.vkHandle(); }

private:
    VulkanAsyncRaiiWrapper<vk::raii::Sampler> m_vkSampler = nullptr;
};

}