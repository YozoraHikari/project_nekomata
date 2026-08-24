module projnekomata;
import projnekomata.corelib;
import :graphics.vulkan.vk_physical_device_props;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_queue_family_swizzling;

namespace projnekomata::gfx::vkrhi {

SwapchainImage::SwapchainImage() = default;
SwapchainImage::SwapchainImage(vk::Image image, vk::Extent2D extent, vk::Format format, VulkanBinarySemaphore&& imagePresentSemaphore) : m_vkImage(image), m_format(format), m_extent(extent), m_vkSemaphoreImagePresent(std::move(imagePresentSemaphore)) {}

auto SwapchainImage::from(vk::Image vkImage, vk::Extent2D extent, vk::Format format) -> SwapchainImage {
    return SwapchainImage(vkImage, extent, format, VulkanBinarySemaphore::create());
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------

static constexpr auto PRESENT_MODE_PRIORITY_VSYNC = StaticSlice<const vk::PresentModeKHR>::inst<vk::PresentModeKHR::eFifoRelaxed, vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate>();
static constexpr auto PRESENT_MODE_PRIORITY_NO_VSYNC = StaticSlice<const vk::PresentModeKHR>::inst<vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifoRelaxed, vk::PresentModeKHR::eFifo>();

VulkanSwapchain::VulkanSwapchain(std::nullptr_t) {}
VulkanSwapchain::VulkanSwapchain(vk::raii::SwapchainKHR&& swapchain, vk::Extent2D swapchainImageExtent, bool hasVsync, Vec<SwapchainImage>&& swapchainImages)
    : m_vkSwapchain(std::move(swapchain)), m_swapchainImageExtent(swapchainImageExtent), m_hasVsync(hasVsync), m_vkSwapchainImages(std::move(swapchainImages)) {}

auto VulkanSwapchain::create(vk::Extent2D windowDrawableExtent, Option<VulkanSwapchain>&& oldSwapchain, bool vsyncEnable) -> VulkanSwapchain {
    auto surfaceProps = VulkanPhysicalDeviceSurfaceProperties::query(VulkanContext::get().vkPhysicalDevice(), VulkanContext::get().vkSurface());

    auto surfaceFormat = surfaceProps.m_surfaceFormats.iter()
        .find([](const auto& sformat) {
            return (sformat.format == vk::Format::eB8G8R8A8Srgb || sformat.format == vk::Format::eR8G8B8A8Srgb)
                && sformat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        })
        .unwrapOr(surfaceProps.m_surfaceFormats[0]);


    auto& presentModePriority = vsyncEnable ? PRESENT_MODE_PRIORITY_VSYNC : PRESENT_MODE_PRIORITY_NO_VSYNC;
    auto presentMode = presentModePriority.iter()
        .find([&](const auto& pmode) { return surfaceProps.m_presentModes.contains(pmode); })
        .unwrapOr(presentModePriority[0]);

    vk::Extent2D imageExtent = surfaceProps.m_capabilities.currentExtent.width == std::numeric_limits<u32>::max() ? windowDrawableExtent : surfaceProps.m_capabilities.currentExtent;

    auto maxImageCount = surfaceProps.m_capabilities.maxImageCount == 0 ? std::numeric_limits<u32>::max() : surfaceProps.m_capabilities.maxImageCount;
    auto imageCount = std::min(surfaceProps.m_capabilities.minImageCount + 1, maxImageCount);
    auto imageSharingMode = VulkanContext::get().vkPhysicalDeviceProps().m_graphicsQueueIndex == VulkanContext::get().vkPhysicalDeviceProps().m_presentQueueIndex ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

    // Reduce noise from window resizing / out-of-date events by checking if this is the first time calling this function is oldSwapchain is nullopt.
    if (oldSwapchain.isNone()) {
        log::info("Swapchain parameters:");
        log::info("  Image count: {}", imageCount);
        log::info("  Image sharing mode: {}", vk::to_string(imageSharingMode));
        log::info("  Image format: {}", vk::to_string(surfaceFormat.format));
        log::info("  Image color space: {}", vk::to_string(surfaceFormat.colorSpace));
        log::info("  Present mode: {}", vk::to_string(presentMode));
    }

    auto queueFamiliesForSwapchain = VulkanContext::get().vkPhysicalDeviceProps().m_queueFamilies[QueueFamily::Graphics | QueueFamily::Present];
    auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR{}
        .setSurface(VulkanContext::get().vkSurface())
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(imageExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eTransferDst)
        .setImageSharingMode(imageSharingMode)
        .setMinImageCount(imageCount)
        .setQueueFamilyIndices(queueFamiliesForSwapchain)
        .setPreTransform(surfaceProps.m_capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setClipped(true)
        .setPresentMode(presentMode);
    
    if (oldSwapchain.isSome()) {
        swapchainCreateInfo.oldSwapchain = oldSwapchain.unwrap().vkSwapchain();
    }

    auto swapchain = vkCheckResult(VulkanContext::get().vkDevice().createSwapchainKHR(swapchainCreateInfo));
    auto preparedSwapchainImages = Vec<vk::Image>::fromStdVector(vkCheckResult(swapchain.getImages()));
    auto swapchainImages = preparedSwapchainImages.iter()
        .map([&](auto&& img) { return SwapchainImage::from(img, imageExtent, surfaceFormat.format); })
        .collect<Vec>();

    return VulkanSwapchain(std::move(swapchain), imageExtent, vsyncEnable, std::move(swapchainImages));
}

auto VulkanSwapchain::acquireNextImage(u64 timeoutNanos, const VulkanBinarySemaphore& imageAcquireSemaphore) -> std::pair<Option<u32>, bool> {
    auto index = m_vkSwapchain.vkHandle().acquireNextImage(timeoutNanos, imageAcquireSemaphore.vkSemaphore());
    if (index.result == vk::Result::eSuccess) {
        return { Some(*index), false };
    }
    if (index.result == vk::Result::eSuboptimalKHR) {
        return { Some(*index), true };
    }

    if (index.result == vk::Result::eErrorOutOfDateKHR) {
        return { None, true };
    }

    panic("vkAcquireNextImageKHR failed");
}

}
