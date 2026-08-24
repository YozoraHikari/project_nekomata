module projnekomata;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_sampler;

namespace projnekomata::gfx::vkrhi {

VulkanSampler::VulkanSampler(std::nullptr_t) {  }
VulkanSampler::VulkanSampler(vk::raii::Sampler&& vkSampler)
    : m_vkSampler(std::move(vkSampler)) {}

auto VulkanSampler::create(vk::Filter minFilter, vk::Filter magFilter, vk::SamplerMipmapMode mipmapMode, vk::SamplerAddressMode addressModeU,
                           vk::SamplerAddressMode addressModeV, vk::SamplerAddressMode addressModeW, bool anisotropyEnable, f32 anisotropy, f32 minLod,
                           f32 maxLod, f32 mipLodBias) -> VulkanSampler {
    auto samplerCreateInfo = vk::SamplerCreateInfo{}
        .setMinFilter(minFilter)
        .setMagFilter(magFilter)
        .setMipmapMode(mipmapMode)
        .setAddressModeU(addressModeU)
        .setAddressModeV(addressModeV)
        .setAddressModeW(addressModeW)
        .setAnisotropyEnable(anisotropyEnable)
        .setMaxAnisotropy(anisotropy)
        .setMinLod(minLod)
        .setMaxLod(maxLod)
        .setMipLodBias(mipLodBias)
        .setCompareEnable(false);

    auto sampler = vkCheckResult(VulkanContext::get().vkDevice().createSampler(samplerCreateInfo));
    return VulkanSampler(std::move(sampler));
}

} // namespace projnekomata::gfx::vkrhi