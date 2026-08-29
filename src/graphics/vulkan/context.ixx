export module projnekomata:graphics.vulkan.context;
import std;
import vulkan;
import vk_mem_alloc;
import projnekomata.corelib;
import :core.platform.sdl;
import :graphics.vulkan.vk_physical_device_props;
import :graphics.vulkan.vk_queue;

export namespace projnekomata::gfx::vkrhi {

constexpr bool kVulkanValidationLayersEnable = false;
constexpr bool kVulkanDebugObjectNamesEnable = true;

constexpr bool kVulkanWantsDebugUtils = kVulkanDebugObjectNamesEnable || kVulkanValidationLayersEnable;

template <typename T> inline auto vkCheckResult(vk::ResultValue<T> x, std::source_location loc = std::source_location::current()) {
    if (x.result != vk::Result::eSuccess) {
        panic("Vulkan result check failed: returned {} (Original check location in {}:{})", vk::to_string(x.result), loc.file_name(), loc.line());
    }
    return std::move(x.value);
}

inline auto vkCheckResult(vk::Result x, std::source_location loc = std::source_location::current()) {
    if (x != vk::Result::eSuccess) {
        panic("Vulkan result check failed: returned {} (Original check location in {}:{})", vk::to_string(x), loc.file_name(), loc.line());
    }
}

enum class AntiLagMethod {
    None,
    AMDAntiLag2,
};
constexpr std::string_view antiLagMethodToString(AntiLagMethod method) {
    switch (method) {
        case AntiLagMethod::None: return "None";
        case AntiLagMethod::AMDAntiLag2: return "AMD Anti-Lag 2";
    }
}

class VulkanContext {
public:
    VulkanContext(std::nullptr_t);
    ~VulkanContext();

    static auto get() -> VulkanContext&;

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;
    
    static auto create(SdlWindow& sdlWindow) -> Unique<VulkanContext>;

    [[nodiscard]] auto vkPhysicalDevice() const -> const vk::raii::PhysicalDevice& { return m_vkPhysicalDevice; }
    [[nodiscard]] auto vkPhysicalDeviceProps() const -> const VulkanPhysicalDeviceProperties& { return m_vkPhysicalDeviceProperties; }
    [[nodiscard]] auto vkSurface() const -> const vk::raii::SurfaceKHR& { return m_vkSurface; }
    [[nodiscard]] auto vkDevice() const -> const vk::raii::Device& { return m_vkDevice; }
    [[nodiscard]] auto vmaAllocator() const -> const vma::raii::Allocator& { return m_vmaAllocator; }

    [[nodiscard]] auto vkQueueGraphics() const -> VulkanQueue& { return *m_vkGraphicsQueue; }
    [[nodiscard]] auto vkQueueAsyncCompute() const -> VulkanQueue& { return *m_vkAsyncComputeQueue; }
    [[nodiscard]] auto vkQueuePresent() const -> VulkanQueue& { return *m_vkPresentQueue; }

    [[nodiscard]] auto vkDeletionQueue() -> Unique<VulkanResourceDeletionQueue>& { return m_vkResourceDeletionQueue; }
    [[nodiscard]] auto shaderCache() -> Unique<class ShaderCache>& { return m_shaderCache; }

    auto antiLagMethod() const -> AntiLagMethod { return m_antiLagMethod.load(std::memory_order_relaxed); }
    auto antiLagPaceInput(u64 frameIndex, u32 targetFps) -> void;
    auto antiLagPacePresent(u64 frameIndex, u32 targetFps) -> void;

    auto currentVramUsage() const -> std::tuple<u64, u64>;
    auto extMemoryBudgetGetVramBudget() const -> u64;

private:

    // TODO: Document
    static auto initVkRaiiContext() -> vk::raii::Context;
    // TODO: Document
    static auto createVkInstance(vk::raii::Context& vkRaiiContext, bool& debuggingEnabled) -> vk::raii::Instance;
    static auto createVkDebugMessenger(const vk::raii::Instance& vkInstance) -> vk::raii::DebugUtilsMessengerEXT;
    // TODO: Document
    static auto createVkSurface(const vk::raii::Instance& vkInstance, projnekomata::SdlWindow& sdlWindow) -> vk::raii::SurfaceKHR;
    // TODO: Document
    static auto pickVkPhysicalDevice(const vk::raii::Instance& vkInstance, const vk::raii::SurfaceKHR& vkSurface) -> std::tuple<vk::raii::PhysicalDevice, VulkanPhysicalDeviceProperties>;
    // TODO: Document
    static auto createVkDevice(const vk::raii::PhysicalDevice& vkPhysicalDevice, const VulkanPhysicalDeviceProperties& vkPhysicalDeviceProps) -> vk::raii::Device;
    // TODO: Document
    static auto createVmaAllocator(const vk::raii::Instance& vkInstance, const vk::raii::PhysicalDevice& vkPhysicalDevice, const VulkanPhysicalDeviceProperties& vkPhysicalDeviceProps, const vk::raii::Device& vkDevice) -> vma::raii::Allocator;

    vk::detail::DynamicLoader m_vkDynamicLoader;
    vk::raii::Context m_vkRaiiContext;

    vk::raii::Instance m_vkInstance = nullptr;
    Option<vk::raii::DebugUtilsMessengerEXT> m_vkDebugMessenger = None;

    vk::raii::SurfaceKHR m_vkSurface = nullptr;
    vk::raii::PhysicalDevice m_vkPhysicalDevice = nullptr;
    VulkanPhysicalDeviceProperties m_vkPhysicalDeviceProperties;
    vk::raii::Device m_vkDevice = nullptr;

    Vec<Unique<VulkanQueue>> m_vkQueues;
    VulkanQueue* m_vkGraphicsQueue{};
    VulkanQueue* m_vkPresentQueue{};
    VulkanQueue* m_vkAsyncComputeQueue{};

    vma::raii::Allocator m_vmaAllocator = nullptr;

    Unique<VulkanResourceDeletionQueue> m_vkResourceDeletionQueue = nullptr;
    Unique<ShaderCache> m_shaderCache                             = nullptr;

    std::atomic<AntiLagMethod> m_antiLagMethod = AntiLagMethod::None;
};

inline VulkanContext* g_vkContext = nullptr;

// ------------------------------------------------------------------------------------------------------------------------------------------------------------

} // namespace projnekomata::gfx::vkrhi