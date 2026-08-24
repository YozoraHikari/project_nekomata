export module projnekomata:graphics.vulkan.vk_swapchain;
import std;
import projnekomata.corelib;
import vulkan;
import :graphics.vulkan.vk_gpu_obrm;
import :graphics.vulkan.sync_primitives.binary_semaphore;
import :graphics.vulkan.vk_image_trait;


export namespace projnekomata::gfx::vkrhi {

class SwapchainImage {
public:
    using isCVulkanImage = std::true_type;

    SwapchainImage();
    SwapchainImage(vk::Image image, vk::Extent2D extent, vk::Format format, VulkanBinarySemaphore&& imagePresentSemaphore);

    static auto from(vk::Image vkImage, vk::Extent2D extent, vk::Format format) -> SwapchainImage;

    SwapchainImage(const SwapchainImage&) = delete;
    SwapchainImage(SwapchainImage&&) = default;
    SwapchainImage& operator=(const SwapchainImage&) = delete;
    SwapchainImage& operator=(SwapchainImage&&) = default;

    [[nodiscard]] auto vkImage() const -> vk::Image { return m_vkImage; }
    [[nodiscard]] auto extent() const -> vk::Extent3D { return vk::Extent3D{ m_extent, 1 }; }
    [[nodiscard]] auto format() const -> vk::Format { return m_format; }
    [[nodiscard]] auto subresourceRangeFull() const -> vk::ImageSubresourceRange { return vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }; }
    [[nodiscard]] auto vkSemaphoreImagePresent() const -> const VulkanBinarySemaphore& { return m_vkSemaphoreImagePresent; }
private:
    // This image doesn't have to be managed by our Vulkan OBRM as it is managed by the driver.
    vk::Image m_vkImage = nullptr;

    vk::Format m_format = vk::Format::eUndefined;
    vk::Extent2D m_extent = vk::Extent2D(0, 0);

    // BUG: it might be still in use by a present queue, which we can't accurately track right now
    // BUG: as a workaround we vkDeviceWaitIdle before recreating or destroying swapchains
    VulkanBinarySemaphore m_vkSemaphoreImagePresent = nullptr;
};
static_assert(CVulkanImage<SwapchainImage> && "SwapchainImage must satisfy CVulkanImage");

// ------------------------------------------------------------------------------------------------------------------------------------------------------------

class VulkanSwapchain {
public:
    VulkanSwapchain(std::nullptr_t);
    VulkanSwapchain(vk::raii::SwapchainKHR&& swapchain, vk::Extent2D swapchainImageExtent, bool hasVsync, Vec<SwapchainImage>&& swapchainImages);

    static auto create(vk::Extent2D windowDrawableExtent, Option<VulkanSwapchain>&& oldSwapchain, bool vsyncEnable) -> VulkanSwapchain;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = default;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = default;

    [[nodiscard]] auto acquireNextImage(u64 timeoutNanos, const VulkanBinarySemaphore& imageAcquireSemaphore) -> std::pair<Option<u32>, bool>;
    [[nodiscard]] auto imageAtIndex(u32 index) -> SwapchainImage& { return m_vkSwapchainImages[index]; }

    [[nodiscard]] auto vkSwapchain() const -> const vk::raii::SwapchainKHR& { return m_vkSwapchain.vkHandle(); }
    [[nodiscard]] auto hasVsync() const -> bool { return m_hasVsync; }

private:
    VulkanAsyncRaiiWrapper<vk::raii::SwapchainKHR> m_vkSwapchain = nullptr;
    vk::Extent2D m_swapchainImageExtent;
    bool m_hasVsync = false;
    Vec<SwapchainImage> m_vkSwapchainImages = Vec<SwapchainImage>::create();
};

}