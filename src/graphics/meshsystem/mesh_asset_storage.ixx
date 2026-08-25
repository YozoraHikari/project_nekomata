export module projnekomata:graphics.meshsystem.mesh_asset_storage;
import std;
import projnekomata.corelib;
import :graphics.meshsystem.pool.mesh_pool;
import :core.containers.freelist_pool;
import :core.math;
import :graphics.meshsystem.handles;

export namespace projnekomata::gfx {
class MeshAssetStorage;

inline MeshAssetStorage* g_meshAssetStorage = nullptr;

class MeshAssetStorage {
public:
    static auto get() -> MeshAssetStorage& { return *g_meshAssetStorage; }
    static auto makeMeshPoolConfig() -> MeshPoolConfig;
    static auto create() -> Unique<MeshAssetStorage>;

    MeshAssetStorage(std::nullptr_t) {}
    MeshAssetStorage();

    auto allocateMeshAsset() -> MeshAsset;
    auto freeMeshAsset(MeshAsset asset) -> void;

    auto getLodList(MeshAsset asset) -> LodList& { return m_lodLists[asset.storageIndex]; }
    auto getLodList(MeshAsset asset) const -> const LodList& { return m_lodLists[asset.storageIndex]; }

    auto perpareLodSpace(MeshAsset asset, u32 lodIndex, usize vertexBufferSize, usize indexBufferSize, usize vertexBufferAlignment, usize indexBufferAlignment) -> void;

    auto tickGC(u64 currentFrameIndex) -> void;
private:

    MeshPool m_meshPool = nullptr;
    VaSlotmap<LodList, 131072> m_lodLists = nullptr;
};

}
