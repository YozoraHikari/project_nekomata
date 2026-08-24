export module projnekomata:graphics.srt.bindless_descriptor_set_srt;
import std;
import vulkan;
import projnekomata.cs;
import :graphics.srt.shader_resource_table;
import :graphics.vulkan.vk_descriptor_pool;
import :graphics.vulkan.vk_descriptor_set_layout;
import :graphics.vulkan.vk_descriptor_set;
import :graphics.vulkan.vk_commands;
import :graphics.vulkan.vk_image;
import :graphics.vulkan.vk_sampler;
import :graphics.vulkan.vk_pipeline_layout;
import :core.containers.abia;

export namespace projnekomata::gfx {

class BindlessDescriptorSetShaderResourceTable : public IShaderResourceTable {
public:
    BindlessDescriptorSetShaderResourceTable(std::nullptr_t);
    BindlessDescriptorSetShaderResourceTable(vkrhi::VulkanDescriptorPool&& descriptorPool, vkrhi::VulkanDescriptorSetLayout&& descriptorSetLayout,
        vkrhi::VulkanDescriptorSet&& descriptorSet, u32 maxSampledImageCount, u32 maxStorageImageCount, u32 maxSamplerCount);

    static auto create(u32 maxSampledImageCount, u32 maxStorageImageCount, u32 maxSamplerCount) -> Unique<BindlessDescriptorSetShaderResourceTable>;

    auto modelName() const -> std::string_view override { return "Bindless"; }

    auto allocateSampledImageIndex() -> SRTResourceIndex override;
    auto allocateSampledImageIndices(u32 count, Slice<SRTResourceIndex> dstIndices) -> void override;
    auto freeSampledImageIndex(SRTResourceIndex index) -> void override;
    auto freeSampledImageIndices(Slice<const SRTResourceIndex> indices) -> void override;

    auto allocateStorageImageIndex() -> SRTResourceIndex override;
    auto allocateStorageImageIndices(u32 count, Slice<SRTResourceIndex> dstIndices) -> void override;
    auto freeStorageImageIndex(SRTResourceIndex index) -> void override;
    auto freeStorageImageIndices(Slice<const SRTResourceIndex> indices) -> void override;

    auto allocateSamplerIndex() -> SRTResourceIndex override;
    auto allocateSamplerIndices(u32 count, Slice<SRTResourceIndex> dstIndices) -> void override;

    auto bindSampledImage(const vkrhi::VulkanImage& image, SRTResourceIndex index) -> void override;
    auto bindSampledImageView(const vkrhi::VulkanImageView& imageView, SRTResourceIndex index) -> void override;
    auto bindStorageImage(const vkrhi::VulkanImage& image, SRTResourceIndex index) -> void override;
    auto bindStorageImageView(const vkrhi::VulkanImageView& imageView, SRTResourceIndex index) -> void override;
    auto bindSampler(const vkrhi::VulkanSampler& sampler, SRTResourceIndex index) -> void override;

    auto bindToCommandBuffer(const vkrhi::VulkanCommandBuffer& cmd, const vkrhi::VulkanPipelineLayout& pipelineLayout, vk::PipelineBindPoint pipelineBindPoint) -> void override;

    auto descriptorSetLayout() const -> const vkrhi::VulkanDescriptorSetLayout& override;

private:
    vkrhi::VulkanDescriptorPool      m_descriptorPool = nullptr;
    vkrhi::VulkanDescriptorSetLayout m_descriptorSetLayout = nullptr;
    vkrhi::VulkanDescriptorSet       m_descriptorSet = nullptr;

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------
    // Allocators

    // Image array index allocator - Because those are dynamic, we need an index allocator that is fully threadsafe and supports reclamation.
    AtomicBitmapIndexAllocator m_sampledImageIndexAllocator = nullptr;
    AtomicBitmapIndexAllocator m_storageImageIndexAllocator = nullptr;

    // Sampler array index allocator - The number of samplers only ever grows, and in a predictable manner, so a plain linear allocator can be used.
    std::atomic<u32> m_nextSamplerIndex = 0;
    u32 m_maxSamplerCount;
};

}