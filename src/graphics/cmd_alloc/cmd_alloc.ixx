export module projnekomata:graphics.cmd_alloc;
import :graphics.vulkan.vk_commands;

export namespace projnekomata::gfx::vkrhi {

// This is currently an implementation with TLS. Maybe it'd make more sense later to make it use a freelist if this becomes a problem

class VulkanCommandPoolsList {
public:
    static auto initThreadLocalCommandPools() -> void;
    static auto destroyThreadLocalCommandPools() -> void;

    static auto getAssignedGraphicsCommandPool() -> VulkanCommandPool& { return tl_graphicsCommandPool; }
    static auto getAssignedAsyncComputeCommandPool() -> VulkanCommandPool& { return tl_asyncComputeCommandPool; }

private:
    inline static thread_local VulkanCommandPool tl_graphicsCommandPool = nullptr;
    inline static thread_local VulkanCommandPool tl_asyncComputeCommandPool = nullptr;
};


}