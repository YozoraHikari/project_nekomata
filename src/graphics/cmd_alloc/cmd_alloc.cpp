module projnekomata;
import :graphics.cmd_alloc;

namespace projnekomata::gfx::vkrhi {

auto VulkanCommandPoolsList::initThreadLocalCommandPools() -> void {
    tl_graphicsCommandPool = VulkanCommandPool::createForGraphics(true);
    tl_asyncComputeCommandPool = VulkanCommandPool::createForAsyncCompute(true);
}

auto VulkanCommandPoolsList::destroyThreadLocalCommandPools() -> void {
    tl_graphicsCommandPool = nullptr;
    tl_asyncComputeCommandPool = nullptr;
}

} // namespace projnekomata::cmdalloc