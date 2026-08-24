export module projnekomata:graphics.vulkan.vk_image;
import std;
import projnekomata.corelib;
import vulkan;
import vk_mem_alloc;
import :graphics.vulkan.vk_image_view;
import :graphics.vulkan.vk_gpu_obrm;
import :graphics.vulkan.vk_image_trait;
import :graphics.vulkan.vk_queue_family_swizzling;
import :graphics.vulkan.context;

export namespace projnekomata::gfx::vkrhi {

struct ImageFormatMd {
    vk::ImageAspectFlags aspectFlags;
    /// for plain uncompressed formats; set to 0 if compressed
    u64 bpp = 0;
    /// for compressed formats; set to 0 if uncompressed
    u64 blockByteSize = 0;
    /// for compressed formats; set to 1 if uncompressed
    u64 blockWidth = 0;
    /// for compressed formats; set to 1 if uncompressed
    u64 blockHeight = 0;
};

class VulkanImageBuilder;
class VulkanImage {
public:
    using isCVulkanImage = std::true_type;

    VulkanImage(std::nullptr_t);
    VulkanImage(vk::raii::Image&& image, vma::raii::Allocation&& allocation, vk::raii::ImageView imageViewWholeSize, vk::ImageSubresourceRange imageSubresourceRangeFull, vk::Extent3D extents, u32 arrayLayerCount, u32 mipLevelCount, bool isCubemap, vk::Format format, vk::ImageType type);


    VulkanImage(const VulkanImage&) = delete;
    VulkanImage(VulkanImage&&) = default;
    VulkanImage& operator=(const VulkanImage&) = delete;
    VulkanImage& operator=(VulkanImage&&) = default;

    constexpr static auto builder() -> VulkanImageBuilder;

    static auto create(vk::ImageType type, vk::Extent3D extent, u32 layerCount, u32 mipLevelCount, bool isCubemap, vk::Format format, vk::ImageUsageFlags usage, vk::ImageTiling tiling, vma::MemoryUsage memoryUsage, vk::MemoryPropertyFlags memoryRequiredFlags, Slice<const u32> queueFamilyIndices, vk::ImageLayout initialLayout) -> VulkanImage;
    static auto createMutableFormat(vk::ImageType type, vk::Extent3D extent, u32 layerCount, u32 mipLevelCount, bool isCubemap, vk::Format format, vk::ImageUsageFlags usage, vk::ImageTiling tiling, vma::MemoryUsage memoryUsage, vk::MemoryPropertyFlags memoryRequiredFlags, Slice<const u32> queueFamilyIndices, vk::ImageLayout initialLayout, Slice<const vk::Format> formats) -> VulkanImage;

    // clang-format off
    static std::unordered_map<vk::Format, ImageFormatMd> s_formatMetadata;
    // clang-format on

    [[nodiscard]] auto vkImage() const -> const vk::raii::Image& { return m_vkImage.vkHandle(); }
    [[nodiscard]] auto vkImageViewWholeSize() const -> const vk::raii::ImageView& { return m_vkImageViewWholeSize.vkHandle(); }
    [[nodiscard]] auto extent() const -> vk::Extent3D { return m_vkImageExtents; }
    [[nodiscard]] auto format() const -> vk::Format { return m_vkImageFormat; }
    [[nodiscard]] auto subresourceRangeFull() const -> vk::ImageSubresourceRange { return m_vkImageSubresourceRangeFull; }

    auto createImageView(u32 baseMipLevel, u32 mipLevelCount, u32 baseArrayLayer, u32 arrayLayerCount, bool keepCube) -> VulkanImageView;
    auto createImageViewWithMinLod(u32 baseMipLevel, u32 mipLevelCount, u32 baseArrayLayer, u32 arrayLayerCount, float minLod, bool keepCube) -> VulkanImageView;

    auto createImageViewWithFormat(vk::Format format, vk::ImageAspectFlags aspectFlags, u32 baseMipLevel, u32 mipLevelCount, u32 baseArrayLayer, u32 arrayLayerCount, bool keepCube) -> VulkanImageView;
    auto createImageViewWithFormatAndMinLod(vk::Format format, vk::ImageAspectFlags aspectFlags, u32 baseMipLevel, u32 mipLevelCount, u32 baseArrayLayer, u32 arrayLayerCount, float minLod, bool keepCube) -> VulkanImageView;

private:
    VulkanAsyncRaiiWrapper<vk::raii::Image> m_vkImage = nullptr;
    VulkanAsyncRaiiWrapper<vk::raii::ImageView> m_vkImageViewWholeSize = nullptr;
    VulkanAsyncRaiiWrapper<vma::raii::Allocation> m_vmaAllocation = nullptr;

    vk::ImageSubresourceRange m_vkImageSubresourceRangeFull;
    vk::Extent3D m_vkImageExtents;
    u32 m_arrayLayerCount;
    u32 m_mipLevelCount;
    vk::Format m_vkImageFormat;
    vk::ImageType m_vkImageType;
    bool m_isCubemap = false;

    static auto selectImageViewType(vk::ImageType type, u32 arrayLayerCount, bool isCubemap) -> vk::ImageViewType;
};
static_assert(CVulkanImage<VulkanImage> && "VulkanImage must satisfy CVulkanImage");

class VulkanImageBuilder {
public:
    constexpr auto type(vk::ImageType type) -> VulkanImageBuilder& { imageCreateInfo().setImageType(type); return *this; }
    constexpr auto extents(vk::Extent3D extents) -> VulkanImageBuilder& { imageCreateInfo().setExtent(extents); return *this; }
    constexpr auto mipLevels(u32 mipLevelCount) -> VulkanImageBuilder& { imageCreateInfo().setMipLevels(mipLevelCount); return *this; }
    constexpr auto arrayLayers(u32 layerCount) -> VulkanImageBuilder& { imageCreateInfo().setArrayLayers(layerCount); return *this; }
    constexpr auto extentsrd(vk::Extent3D extents, u32 layerCount, u32 mipLevelCount) -> VulkanImageBuilder&
        { imageCreateInfo().setExtent(extents); imageCreateInfo().setArrayLayers(layerCount); imageCreateInfo().setMipLevels(mipLevelCount); return *this; }
    constexpr auto format(vk::Format format) -> VulkanImageBuilder& { imageCreateInfo().setFormat(format); return *this; }
    constexpr auto usage(vk::ImageUsageFlags usage) -> VulkanImageBuilder& { imageCreateInfo().setUsage(usage); return *this; }
    constexpr auto tiling(vk::ImageTiling tiling) -> VulkanImageBuilder& { imageCreateInfo().setTiling(tiling); return *this; }
    constexpr auto samples(vk::SampleCountFlagBits samples) -> VulkanImageBuilder& { imageCreateInfo().setSamples(samples); return *this; }
    constexpr auto initialLayout(vk::ImageLayout initialLayout) -> VulkanImageBuilder& { imageCreateInfo().setInitialLayout(initialLayout); return *this; }
    constexpr auto memoryUsage(vma::MemoryUsage memoryUsage) -> VulkanImageBuilder& { m_allocationCreateInfo.usage = memoryUsage; return *this; }
    constexpr auto memoryRequiredFlags(vk::MemoryPropertyFlags memoryRequiredFlags) -> VulkanImageBuilder& { m_allocationCreateInfo.requiredFlags = memoryRequiredFlags; return *this; }
    constexpr auto queueFamilyIndices(Slice<const u32> queueFamilyIndices) -> VulkanImageBuilder& { imageCreateInfo().setQueueFamilyIndices(queueFamilyIndices); return *this; }
    constexpr auto queueFamilyIndices(QueueFamily families) -> VulkanImageBuilder& { auto queues = VulkanContext::get().vkPhysicalDeviceProps().m_queueFamilies[families]; imageCreateInfo().setQueueFamilyIndices(queues); return *this; }
    constexpr auto isCubemap(bool isCubemap) -> VulkanImageBuilder& { m_isCubemap = isCubemap; return *this; }
    constexpr auto mutableFormat(Slice<const vk::Format> formats) -> VulkanImageBuilder& { imageCreateInfo().flags |= vk::ImageCreateFlagBits::eMutableFormat; m_imageCreateInfoChain.get<vk::ImageFormatListCreateInfo>().setViewFormats(formats); return *this; }

    constexpr auto build() -> VulkanImage {
        if (m_isCubemap) {
            imageCreateInfo().flags |= vk::ImageCreateFlagBits::eCubeCompatible;
            imageCreateInfo().arrayLayers *= 6;
        }
        
        imageCreateInfo().sharingMode = imageCreateInfo().queueFamilyIndexCount == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
        
        auto [allocation, image] = vkCheckResult(VulkanContext::get().vmaAllocator().createImage(imageCreateInfo(), m_allocationCreateInfo)).split();
        
        auto imageViewType = selectFullImageViewType();
        auto imageViewSubresRange = vk::ImageSubresourceRange{}
            .setBaseMipLevel(0)
            .setLevelCount(imageCreateInfo().mipLevels)
            .setBaseArrayLayer(0)
            .setLayerCount(imageCreateInfo().arrayLayers)
            .setAspectMask(VulkanImage::s_formatMetadata[imageCreateInfo().format].aspectFlags);

        auto imageViewCreateInfo = vk::ImageViewCreateInfo{}
            .setImage(image)
            .setViewType(imageViewType)
            .setFormat(imageCreateInfo().format)
            .setSubresourceRange(imageViewSubresRange);

        auto imageView = vkCheckResult(VulkanContext::get().vkDevice().createImageView(imageViewCreateInfo));
        return VulkanImage(std::move(image), std::move(allocation), std::move(imageView), imageViewSubresRange, imageCreateInfo().extent, imageCreateInfo().arrayLayers, imageCreateInfo().mipLevels, m_isCubemap, imageCreateInfo().format, imageCreateInfo().imageType);
    }


private:
    constexpr VulkanImageBuilder() = default;

    vk::StructureChain<vk::ImageCreateInfo, vk::ImageFormatListCreateInfo> m_imageCreateInfoChain = {};
    vma::AllocationCreateInfo m_allocationCreateInfo = {};
    bool m_isCubemap = false;

    auto imageCreateInfo() -> vk::ImageCreateInfo& { return m_imageCreateInfoChain.get<vk::ImageCreateInfo>(); }
    auto selectFullImageViewType() -> vk::ImageViewType {
        if (m_isCubemap) return vk::ImageViewType::eCube;

        vk::ImageViewType imageViewType = vk::ImageViewType::e2D;
        switch (imageCreateInfo().imageType) {
            case vk::ImageType::e1D: imageViewType = vk::ImageViewType::e1D; break;
            case vk::ImageType::e2D: imageViewType = vk::ImageViewType::e2D; break;
            case vk::ImageType::e3D: imageViewType = vk::ImageViewType::e3D; break;
        }

        if (imageCreateInfo().arrayLayers > 1) {
            if (imageViewType == vk::ImageViewType::e1D) imageViewType = vk::ImageViewType::e1DArray;
            if (imageViewType == vk::ImageViewType::e2D) imageViewType = vk::ImageViewType::e2DArray;
        }
        return imageViewType;
    }

    friend class VulkanImage;
};
constexpr auto VulkanImage::builder() -> VulkanImageBuilder { return VulkanImageBuilder(); }

}
