module projnekomata;
import projnekomata.corelib;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_queue_family_swizzling;
import :graphics.meshsystem.mesh_asset_storage;

namespace projnekomata::gfx {

auto MeshAssetStorage::makeMeshPoolConfig() -> MeshPoolConfig {
    MeshPoolConfig poolConfig{};
    poolConfig.dedicatedAllocationThreshold = 8 * 1024 * 1024;
    poolConfig.defaultDeferFrames = 4;
    poolConfig.maxSlabCount = 16;
    poolConfig.slabSize = 16 * 1024 * 1024;
    poolConfig.queueFamilyIndices = vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_queueFamilies[vkrhi::QueueFamily::Graphics | vkrhi::QueueFamily::AsyncCompute];
    return poolConfig;
}

MeshAssetStorage::MeshAssetStorage() : m_meshPool(makeMeshPoolConfig()), m_lodLists(FreelistPoolV2<LodList, 131072>::create()) {}

auto MeshAssetStorage::create() -> Unique<MeshAssetStorage> {
    debug_assert(g_meshAssetStorage == nullptr, "only one MeshAssetStorage may live at any given time");
    auto meshAssetStorage = Unique<MeshAssetStorage>::create();
    g_meshAssetStorage = meshAssetStorage.ptr();
    return meshAssetStorage;
}

auto MeshAssetStorage::allocateMeshAsset() -> MeshAsset {
    auto storageIndex = m_lodLists.emplace();
    return MeshAsset{storageIndex};
}

auto MeshAssetStorage::freeMeshAsset(MeshAsset asset) -> void {
    m_lodLists.free(asset.storageIndex);
}

auto MeshAssetStorage::perpareLodSpace(MeshAsset asset, u32 lodIndex, usize vertexBufferSize, usize indexBufferSize, usize vertexBufferAlignment, usize indexBufferAlignment) -> void {
    getLodList(asset).lods[lodIndex].meshSuballocation = m_meshPool.allocateMesh(vertexBufferSize, indexBufferSize, vertexBufferAlignment, indexBufferAlignment);
}
auto MeshAssetStorage::tickGC(u64 currentFrameIndex) -> void {
    m_meshPool.tickGC(currentFrameIndex);
}

} // namespace projnekomata::meshsystem