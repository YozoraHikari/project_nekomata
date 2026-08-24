export module projnekomata:graphics.meshsystem.handles;
import projnekomata.cs;
import std;
import :graphics.meshsystem.pool.mesh_pool;
import :core.math;


export namespace projnekomata {

constexpr u32 kLodListMaxLodCount = 4;

struct MeshAsset {
    usize storageIndex;
};

// todo: RAII semantics (refcounting)
struct Lod {
    gfx::MeshSuballocation meshSuballocation = {};
    float screenSizeThreshold = 0.0f;
};

struct LodList {
    std::array<Lod, kLodListMaxLodCount> lods = {};

    /// Specifies the highest LOD level for the mesh.
    u32 maxLodIndex = 0;

    float boundingSphereRadius = 1.0f;
    float lodHysteresisFactor = 1.1f;

    /// Specifies the current index of the best available LOD. If set to ~0, then no LODs are available.
    std::atomic<u32> bestLodIndex = ~0;

    LodList() = default;
    LodList(const LodList& other) = delete;
    LodList& operator=(const LodList& other) = delete;
    LodList(LodList&& other) noexcept
        : lods(std::move(other.lods)), maxLodIndex(std::exchange(other.maxLodIndex, 0u)), bestLodIndex(other.bestLodIndex.exchange(~0u, std::memory_order_relaxed)) {}
    LodList& operator=(LodList&& other) noexcept {
        if (this == &other) return *this;

        lods = std::move(other.lods);
        maxLodIndex = std::exchange(other.maxLodIndex, 0u);
        bestLodIndex = other.bestLodIndex.exchange(~0u, std::memory_order_relaxed);
        return *this;
    }

    float computeScreenSpaceError(math::Vector3f objectPos, math::Vector3f cameraPos, float perspectiveFocalLength, float objectUniformScale) const {
        float distance = (objectPos - cameraPos).length();
        if (distance < math::consts::EPSILON) return std::numeric_limits<float>::infinity();
        return (2.0f * objectUniformScale * boundingSphereRadius / distance) * perspectiveFocalLength;
    }

};

}
