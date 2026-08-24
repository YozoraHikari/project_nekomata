export module projnekomata:graphics.vulkan.vk_commands;
import std;
import projnekomata.cs;
import vulkan;
import :graphics.vulkan.vk_gpu_obrm;

export namespace projnekomata::gfx::vkrhi {

struct ResettableCbuf;
struct NoResettableCbuf;
template <typename T> concept SetsResettable = std::same_as<T, ResettableCbuf> || std::same_as<T, NoResettableCbuf>;

struct Graphics;
struct NoGraphics;
template <typename T> concept SetsGraphics = std::same_as<T, Graphics> || std::same_as<T, NoGraphics>;

struct Compute;
struct NoCompute;
template <typename T> concept SetsCompute = std::same_as<T, Compute> || std::same_as<T, NoCompute>;

class VulkanCommandBuffer {
public:
    VulkanCommandBuffer(std::nullptr_t);
    VulkanCommandBuffer(vk::raii::CommandBuffer&& vkCommandBuffer);

    VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer(VulkanCommandBuffer&&) = default;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer&&) = default;

    [[nodiscard]] auto vkCommandBuffer() const -> const vk::raii::CommandBuffer& { return m_vkCommandBuffer.vkHandle(); }

private:
    VulkanAsyncRaiiWrapper<vk::raii::CommandBuffer> m_vkCommandBuffer = nullptr;
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------

//template<SetsResettable IsResettable, SetsGraphics IsGraphics, SetsCompute IsCompute>
class VulkanCommandPool {
public:
    VulkanCommandPool(std::nullptr_t);
    VulkanCommandPool(vk::raii::CommandPool&& vkCommandPool);

    static auto createForGraphics(bool transientCommandBuffers) -> VulkanCommandPool;
    static auto createForAsyncCompute(bool transientCommandBuffers) -> VulkanCommandPool;

    VulkanCommandPool(const VulkanCommandPool&) = delete;
    VulkanCommandPool(VulkanCommandPool&&) = default;
    VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(VulkanCommandPool&&) = default;

    auto reset() -> void;
    auto allocateCommandBuffer(vk::CommandBufferLevel level) -> VulkanCommandBuffer;
    [[nodiscard]] auto vkCommandPool() const -> const vk::raii::CommandPool& { return m_vkCommandPool.vkHandle(); }

private:
    static auto createWithQueueFamilyIndex(u32 queueFamilyIndex, bool transientCommandBuffers) -> VulkanCommandPool;

    VulkanAsyncRaiiWrapper<vk::raii::CommandPool> m_vkCommandPool = nullptr;
};

}