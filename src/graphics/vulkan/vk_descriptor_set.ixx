export module projnekomata:graphics.vulkan.vk_descriptor_set;
import std;
import vulkan;
import projnekomata.cs;
import :graphics.vulkan.vk_gpu_obrm;
import :graphics.vulkan.vk_image;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_sampler;

export namespace projnekomata::gfx::vkrhi {

class VulkanDescriptorSet {
public:
    VulkanDescriptorSet(std::nullptr_t);
    VulkanDescriptorSet(vk::raii::DescriptorSet&& vkDescriptorSet);
    ~VulkanDescriptorSet();

    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet(VulkanDescriptorSet&&) = default;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&&) = default;

    [[nodiscard]] auto vkDescriptorSet() const -> const vk::raii::DescriptorSet& { return m_vkDescriptorSet.vkHandle(); }

private:
    VulkanAsyncRaiiWrapper<vk::raii::DescriptorSet> m_vkDescriptorSet = nullptr;
    bool m_isFromFreeDescriptorSetPool = true;
};

class VulkanDescriptorSetWriter {
public:
    constexpr VulkanDescriptorSetWriter(VulkanDescriptorSet& vkDescriptorSet)
        : m_vkDescriptorSet(vkDescriptorSet) {}

    VulkanDescriptorSetWriter(const VulkanDescriptorSetWriter&) = delete;
    VulkanDescriptorSetWriter(VulkanDescriptorSetWriter&&) = default;
    VulkanDescriptorSetWriter& operator=(const VulkanDescriptorSetWriter&) = delete;
    VulkanDescriptorSetWriter& operator=(VulkanDescriptorSetWriter&&) = default;

    [[nodiscard]] constexpr auto bindSampledImage(u32 binding, u32 dstDescriptorIndex, const VulkanImage& image) -> VulkanDescriptorSetWriter& {
        auto imageInfo = vk::DescriptorImageInfo{}
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSampler(nullptr)
            .setImageView(image.vkImageViewWholeSize());
        m_descriptorImageInfos.emplace(binding, dstDescriptorIndex, vk::DescriptorType::eSampledImage, imageInfo);
        return *this;
    }
    [[nodiscard]] constexpr auto bindSampledImage(u32 binding, u32 dstDescriptorIndex, const VulkanImageView& imageView) -> VulkanDescriptorSetWriter& {
        auto imageInfo = vk::DescriptorImageInfo{}
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSampler(nullptr)
            .setImageView(imageView.vkImageView());
        m_descriptorImageInfos.emplace(binding, dstDescriptorIndex, vk::DescriptorType::eSampledImage, imageInfo);
        return *this;
    }

    [[nodiscard]] constexpr auto bindStorageImage(u32 binding, u32 dstDescriptorIndex, const VulkanImage& image) -> VulkanDescriptorSetWriter& {
        auto imageInfo = vk::DescriptorImageInfo{}
            .setImageLayout(vk::ImageLayout::eGeneral)
            .setSampler(nullptr)
            .setImageView(image.vkImageViewWholeSize());
        m_descriptorImageInfos.emplace(binding, dstDescriptorIndex, vk::DescriptorType::eStorageImage, imageInfo);
        return *this;
    }
    [[nodiscard]] constexpr auto bindStorageImage(u32 binding, u32 dstDescriptorIndex, const VulkanImageView& imageView) -> VulkanDescriptorSetWriter& {
        auto imageInfo = vk::DescriptorImageInfo{}
            .setImageLayout(vk::ImageLayout::eGeneral)
            .setSampler(nullptr)
            .setImageView(imageView.vkImageView());
        m_descriptorImageInfos.emplace(binding, dstDescriptorIndex, vk::DescriptorType::eStorageImage, imageInfo);
        return *this;
    }

    [[nodiscard]] constexpr auto bindInputAttachment(u32 binding, u32 dstDescriptorIndex, const VulkanImage& imageView) -> VulkanDescriptorSetWriter& {
        auto imageInfo = vk::DescriptorImageInfo{}
        .setImageLayout(vk::ImageLayout::eRenderingLocalRead)
        .setSampler(nullptr)
        .setImageView(imageView.vkImageViewWholeSize());
        m_descriptorImageInfos.emplace(binding, dstDescriptorIndex, vk::DescriptorType::eInputAttachment, imageInfo);
        return *this;
    }
    [[nodiscard]] constexpr auto bindInputAttachment(u32 binding, u32 dstDescriptorIndex, const VulkanImageView& imageView) -> VulkanDescriptorSetWriter& {
        auto imageInfo = vk::DescriptorImageInfo{}
            .setImageLayout(vk::ImageLayout::eRenderingLocalRead)
            .setSampler(nullptr)
            .setImageView(imageView.vkImageView());
        m_descriptorImageInfos.emplace(binding, dstDescriptorIndex, vk::DescriptorType::eInputAttachment, imageInfo);
        return *this;
    }

    [[nodiscard]] constexpr auto bindSampler(u32 binding, u32 dstDescriptorIndex, const VulkanSampler& sampler) -> VulkanDescriptorSetWriter& {
        auto imageInfo = vk::DescriptorImageInfo{}
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSampler(sampler.vkSampler())
            .setImageView(nullptr);
        m_descriptorImageInfos.emplace(binding, dstDescriptorIndex, vk::DescriptorType::eSampler, imageInfo);
        return *this;
    }

    constexpr auto commit() -> void {
        Vec<vk::WriteDescriptorSet> writeDescriptorSets;

        for (auto& [binding, dstDescriptorIndex, dtype, imageInfo] : m_descriptorImageInfos) {

            auto writeDescriptorSet = vk::WriteDescriptorSet{}
                .setDstSet(m_vkDescriptorSet.get().vkDescriptorSet())
                .setDstBinding(binding)
                .setDstArrayElement(dstDescriptorIndex)
                .setDescriptorType(dtype)
                .setDescriptorCount(1)
                .setPImageInfo(&imageInfo);
            writeDescriptorSets.emplace(writeDescriptorSet);
        }

        VulkanContext::get().vkDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
    }

private:
    std::reference_wrapper<VulkanDescriptorSet> m_vkDescriptorSet;

    Vec<std::tuple<u32, u32, vk::DescriptorType, vk::DescriptorImageInfo>> m_descriptorImageInfos;
};

}