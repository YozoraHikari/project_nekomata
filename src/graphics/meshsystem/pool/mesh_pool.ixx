export module projnekomata:graphics.meshsystem.pool.mesh_pool;
import std;
import projnekomata.corelib;
import vulkan;
import vk_mem_alloc;
import :graphics.vulkan.vk_buffer;

export namespace projnekomata::gfx {

struct SlabAllocationRef {
    /// Index of the slab within the pool allocated from
    u32 slabIndex = ~0;
    vma::raii::VirtualAllocation virtualAllocation = nullptr;
    bool isDedicated = false;
};

struct BufferPoolSuballocation {
    /// The vk::Buffer to bind when using the allocation.
    vk::Buffer buffer = nullptr;
    /// Byte offset within the buffer where the suballocation is placed.
    vk::DeviceSize offset = 0;
    /// Size of the suballocation.
    vk::DeviceSize size = 0;
    /// Host pointer to the suballocation. Note this is only valid for buffers created with VulkanBufferMemoryMapping set to map the buffer somehow.
    u8* hostAddress = nullptr;
    /// Device pointer to the suballocation. Note this is only valid for buffers created with eShaderDeviceAddress usage flag (e.g., vertex buffers).
    vk::DeviceAddress deviceAddress = 0;

    SlabAllocationRef slabAllocationRef;

    [[nodiscard]] bool isValid() const { return buffer != nullptr; }
    [[nodiscard]] vk::DeviceAddress baseDeviceAddress() const { return deviceAddress - offset; }
    [[nodiscard]] u8* baseHostAddress() const { return hostAddress - offset; }
};

struct BufferPoolStats {
    u64 totalCapacityBytes = 0;
    u64 usedBytes = 0;
    u32 activeSlabs = 0;
    u32 dedicatedAllocations = 0;
    u32 pendingFrees = 0;
};

struct BufferPoolConfig {
    u64 slabSize = 16ull * 1024 * 1024;
    u32 maxSlabCount = 16;
    u64 dedicatedAllocationThreshold = 8 * 1024 * 1024;
    u32 defaultDeferFrames = 4;

    vk::BufferUsageFlags bufferUsageFlags;
    Slice<const u32> queueFamilyIndices;
    vma::MemoryUsage memoryUsage;
    vk::MemoryPropertyFlags memoryRequiredFlags;
    vkrhi::VulkanBufferMemoryMapping hostMemoryMapping;
};

class BufferPool {
public:
    BufferPool(std::nullptr_t);
    BufferPool(const BufferPoolConfig& cfg);
    ~BufferPool();

    BufferPool(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool& operator=(BufferPool&&) = delete;

    [[nodiscard]] auto allocate(u64 byteSize, u64 alignment) -> BufferPoolSuballocation;
    auto free(BufferPoolSuballocation& alloc, u64 currentFrameIndex) -> void;
    auto tickGC(u64 currentFrameIndex) -> void;

    [[nodiscard]] auto stats() const -> BufferPoolStats;

private:
    struct Slab {
        vkrhi::VulkanBuffer buffer = nullptr;
        vma::raii::VirtualBlock virtualBlock = nullptr;
        u64 usedBytes = 0;
        Slab(vkrhi::VulkanBuffer&& buf, vma::raii::VirtualBlock&& virtualBlk) : buffer(std::move(buf)), virtualBlock(std::move(virtualBlk)) {}

        static auto create(const BufferPoolConfig& cfg) -> Slab;

        Slab() = default;
        Slab(Slab&&) noexcept = default;
        Slab& operator=(Slab&&) noexcept = default;
    };

    struct DedicatedAllocation {
        Unique<vkrhi::VulkanBuffer> buffer = nullptr;

        DedicatedAllocation(Unique<vkrhi::VulkanBuffer>&& buf) : buffer(std::move(buf)) {}

        static auto create(u64 byteSize, const BufferPoolConfig& cfg) -> DedicatedAllocation;
    };

    struct PendingFree {
        SlabAllocationRef slabAllocationRef;
        u64 deferredTillFrameIndex = 0;
    };

    [[nodiscard]] Option<BufferPoolSuballocation> trySuballocate(u32 slabIndex, u64 byteSize, u64 alignment);

    auto processFree(const SlabAllocationRef& pendingFree) -> void;

    static constexpr u32 kDedicatedAllocationFlagBit = 0x80000000u;

    BufferPoolConfig m_cfg;

    Vec<Slab> m_slabs;
    Vec<DedicatedAllocation> m_dedicatedAllocations;
    std::deque<PendingFree> m_pendingFrees;

    mutable std::mutex m_allocatorMutex;
    mutable std::mutex m_gcMutex;
};

struct MeshSuballocation {
    BufferPoolSuballocation vertexBuffer;
    BufferPoolSuballocation indexBuffer;
};

struct MeshPoolConfig {
    u64 slabSize = 16ull * 1024 * 1024;
    u32 maxSlabCount = 16;
    u64 dedicatedAllocationThreshold = 8 * 1024 * 1024;
    u32 defaultDeferFrames = 4;
    Slice<const u32> queueFamilyIndices;
};

class MeshPool {
public:
    MeshPool(std::nullptr_t);
    MeshPool(const MeshPoolConfig& cfg);

    auto allocateVertexBuffer(u64 byteSize, u64 alignment) -> BufferPoolSuballocation;
    auto allocateIndexBuffer(u64 byteSize, u64 alignment) -> BufferPoolSuballocation;
    auto freeVertexBuffer(BufferPoolSuballocation& alloc, u64 currentFrameIndex) -> void;
    auto freeIndexBuffer(BufferPoolSuballocation& alloc, u64 currentFrameIndex) -> void;

    auto allocateMesh(u64 vertexBufferSize, u64 indexBufferSize, u64 vertexAlignment, u64 indexAlignment) -> MeshSuballocation;
    auto freeMesh(MeshSuballocation& alloc, u64 currentFrameIndex) -> void;

    auto tickGC(u64 currentFrameIndex) -> void;

private:
    static auto bufferPoolBaseCfg(const MeshPoolConfig& cfg) -> BufferPoolConfig;
    static auto makeVertexBufferPool(const MeshPoolConfig& cfg) -> BufferPool;
    static auto makeIndexBufferPool(const MeshPoolConfig& cfg) -> BufferPool;

    BufferPool m_vertexBufferPool = nullptr;
    BufferPool m_indexBufferPool = nullptr;
};

} // namespace projnekomata