module projnekomata;
import :graphics.meshsystem.pool.mesh_pool;

namespace projnekomata::gfx {

BufferPool::BufferPool(std::nullptr_t) {  }
BufferPool::BufferPool(const BufferPoolConfig& cfg) : m_cfg(cfg) {}

BufferPool::~BufferPool() {
    std::scoped_lock lock(m_gcMutex);

    for (const auto& freeOp : m_pendingFrees)
        processFree(freeOp.slabAllocationRef);

    m_pendingFrees.clear();
}

auto BufferPool::Slab::create(const BufferPoolConfig& cfg) -> Slab {
    auto buffer = vkrhi::VulkanBuffer::builder()
        .len(cfg.slabSize)
        .usage(cfg.bufferUsageFlags)
        .memoryUsage(cfg.memoryUsage)
        .memoryMapping(cfg.hostMemoryMapping)
        .memoryRequiredFlags(cfg.memoryRequiredFlags)
        .queueFamilyIndices(cfg.queueFamilyIndices)
        .build();

    auto vbCreateInfo = vma::VirtualBlockCreateInfo{}
        .setSize(cfg.slabSize);

    auto block = vkrhi::vkCheckResult(vma::raii::createVirtualBlock(vbCreateInfo));
    return Slab(std::move(buffer), std::move(block));
}

auto BufferPool::DedicatedAllocation::create(u64 byteSize, const BufferPoolConfig& cfg) -> DedicatedAllocation {
    auto buffer = vkrhi::VulkanBuffer::builder()
        .len(byteSize)
        .usage(cfg.bufferUsageFlags)
        .memoryUsage(cfg.memoryUsage)
        .memoryMapping(cfg.hostMemoryMapping)
        .memoryRequiredFlags(cfg.memoryRequiredFlags)
        .queueFamilyIndices(cfg.queueFamilyIndices)
        .build();

    auto bufferptr = Unique<vkrhi::VulkanBuffer>::create(std::move(buffer));
    return DedicatedAllocation(std::move(bufferptr));
}

Option<BufferPoolSuballocation> BufferPool::trySuballocate(u32 slabIndex, u64 byteSize, u64 alignment) {
    Slab& slab = m_slabs[slabIndex];

    auto vaCreateInfo = vma::VirtualAllocationCreateInfo{}
        .setSize(byteSize)
        .setAlignment(alignment);

    vma::raii::VirtualAllocation va = vkrhi::vkCheckResult(slab.virtualBlock.allocate(vaCreateInfo));

    slab.usedBytes += byteSize;

    auto allocOffset = va.getInfo().offset;

    BufferPoolSuballocation suballocation{};
    suballocation.buffer = slab.buffer.vkBuffer();
    suballocation.offset = allocOffset;
    suballocation.size = byteSize;

    if (m_cfg.hostMemoryMapping != vkrhi::VulkanBufferMemoryMapping::DontMap) {
        suballocation.hostAddress = slab.buffer.memoryHostPtr() + allocOffset;
    }

    if (m_cfg.bufferUsageFlags & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        suballocation.deviceAddress = slab.buffer.memoryDevicePtr() + allocOffset;
    }

    suballocation.slabAllocationRef.isDedicated = false;
    suballocation.slabAllocationRef.slabIndex = slabIndex;
    suballocation.slabAllocationRef.virtualAllocation = std::move(va);

    return Some(std::move(suballocation));
}

auto BufferPool::allocate(u64 byteSize, u64 alignment) -> BufferPoolSuballocation {
    std::scoped_lock lock(m_allocatorMutex);

    bool isDedicatedAlloc = (byteSize >= m_cfg.dedicatedAllocationThreshold) || (byteSize >= m_cfg.slabSize);

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Path 1 - Dedicated path

    if (isDedicatedAlloc) {
        auto dedicatedAlloc = DedicatedAllocation::create(byteSize, m_cfg);

        BufferPoolSuballocation suballocation{};
        suballocation.buffer = dedicatedAlloc.buffer->vkBuffer();
        suballocation.offset = 0;
        suballocation.size = byteSize;
        if (m_cfg.hostMemoryMapping != vkrhi::VulkanBufferMemoryMapping::DontMap) {
            suballocation.hostAddress = dedicatedAlloc.buffer->memoryHostPtr();
        }
        if (m_cfg.bufferUsageFlags & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
            suballocation.deviceAddress = dedicatedAlloc.buffer->memoryDevicePtr();
        }

        u32 idx = static_cast<u32>(m_dedicatedAllocations.size());
        for (u32 i = 0; i < static_cast<u32>(m_dedicatedAllocations.size()); i++) {
            if (!m_dedicatedAllocations[i].buffer.isNull()) { idx = i; break; }
        }
        if (idx == static_cast<u32>(m_dedicatedAllocations.size()))
            m_dedicatedAllocations.emplace(std::move(dedicatedAlloc));
        else
            m_dedicatedAllocations[idx] = std::move(dedicatedAlloc);

        suballocation.slabAllocationRef.isDedicated = true;
        suballocation.slabAllocationRef.slabIndex = idx | kDedicatedAllocationFlagBit;

        return suballocation;
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Path 2 - Try existing slabs

    for (u32 i = 0; i < m_slabs.size(); i++) {
        if (auto suballocation = trySuballocate(i, byteSize, alignment)) return std::move(suballocation.unwrap());
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Path 3 - Grow slabs

    if (m_slabs.size() >= m_cfg.maxSlabCount) {
        panic("ran out of slabs space");
    }

    m_slabs.emplace(Slab::create(m_cfg));

    if (auto suballocation = trySuballocate(m_slabs.size() - 1, byteSize, alignment)) return std::move(suballocation.unwrap());

    panic("ran out of slabs space");
}

auto BufferPool::free(BufferPoolSuballocation& alloc, u64 currentFrameIndex) -> void {
    if (!alloc.isValid()) return;

    PendingFree freeOp{};
    freeOp.slabAllocationRef = std::move(alloc.slabAllocationRef);
    freeOp.deferredTillFrameIndex = currentFrameIndex + m_cfg.defaultDeferFrames;

    {
        std::scoped_lock lock(m_gcMutex);
        m_pendingFrees.push_back(std::move(freeOp));
    }

    alloc = {};
}

auto BufferPool::processFree(const SlabAllocationRef& pendingFree) -> void {
    std::scoped_lock lock(m_allocatorMutex);

    if (pendingFree.isDedicated) {
        u32 index = pendingFree.slabIndex & ~kDedicatedAllocationFlagBit;
        m_dedicatedAllocations[index].buffer.release();
        return;
    }

    Slab& slab = m_slabs[pendingFree.slabIndex];

    slab.usedBytes -= pendingFree.virtualAllocation.getInfo().size;
}

auto BufferPool::tickGC(u64 currentFrameIndex) -> void {
    std::scoped_lock lock(m_gcMutex);

    while (!m_pendingFrees.empty() && m_pendingFrees.front().deferredTillFrameIndex <= currentFrameIndex) {
        processFree(m_pendingFrees.front().slabAllocationRef);
        m_pendingFrees.pop_front();
    }
}

auto BufferPool::stats() const -> BufferPoolStats {
    BufferPoolStats s{};

    {
        std::scoped_lock lock(m_allocatorMutex);
        s.activeSlabs = static_cast<u32>(m_slabs.size());
        for (const auto& slab : m_slabs) {
            s.totalCapacityBytes += slab.buffer.size();
            s.usedBytes          += slab.usedBytes;
        }
        for (const auto& ded : m_dedicatedAllocations) {
            if (!ded.buffer.isNull()) {
                s.dedicatedAllocations++;
                s.totalCapacityBytes += ded.buffer->size();
                s.usedBytes          += ded.buffer->size();
            }
        }
    }

    {
        std::scoped_lock lock(m_gcMutex);
        s.pendingFrees = static_cast<u32>(m_pendingFrees.size());
    }
    return s;
}

MeshPool::MeshPool(std::nullptr_t) {  }
MeshPool::MeshPool(const MeshPoolConfig& cfg)
    : m_vertexBufferPool(makeVertexBufferPool(cfg)), m_indexBufferPool(makeIndexBufferPool(cfg)) {}

auto MeshPool::allocateVertexBuffer(u64 byteSize, u64 alignment) -> BufferPoolSuballocation {
    return m_vertexBufferPool.allocate(byteSize, alignment);
}
auto MeshPool::allocateIndexBuffer(u64 byteSize, u64 alignment) -> BufferPoolSuballocation {
    return m_indexBufferPool.allocate(byteSize, alignment);
}
auto MeshPool::freeVertexBuffer(BufferPoolSuballocation& alloc, u64 currentFrameIndex) -> void {
    m_vertexBufferPool.free(alloc, currentFrameIndex);
}
auto MeshPool::freeIndexBuffer(BufferPoolSuballocation& alloc, u64 currentFrameIndex) -> void {
    m_indexBufferPool.free(alloc, currentFrameIndex);
}

auto MeshPool::allocateMesh(u64 vertexBufferSize, u64 indexBufferSize, u64 vertexAlignment, u64 indexAlignment) -> MeshSuballocation {
    MeshSuballocation alloc{};
    alloc.vertexBuffer = m_vertexBufferPool.allocate(vertexBufferSize, vertexAlignment);
    alloc.indexBuffer = m_indexBufferPool.allocate(indexBufferSize, indexAlignment);
    return alloc;
}
auto MeshPool::freeMesh(MeshSuballocation& alloc, u64 currentFrameIndex) -> void {
    m_vertexBufferPool.free(alloc.vertexBuffer, currentFrameIndex);
    m_indexBufferPool.free(alloc.indexBuffer, currentFrameIndex);
}

auto MeshPool::tickGC(u64 currentFrameIndex) -> void {
    m_vertexBufferPool.tickGC(currentFrameIndex);
    m_indexBufferPool.tickGC(currentFrameIndex);
}

auto MeshPool::bufferPoolBaseCfg(const MeshPoolConfig& cfg) -> BufferPoolConfig {
    return BufferPoolConfig{
        .slabSize                     = cfg.slabSize,
        .maxSlabCount                 = cfg.maxSlabCount,
        .dedicatedAllocationThreshold = cfg.dedicatedAllocationThreshold,
        .defaultDeferFrames           = cfg.defaultDeferFrames,
        .bufferUsageFlags             = {},
        .queueFamilyIndices           = cfg.queueFamilyIndices,
        .memoryUsage                  = vma::MemoryUsage::eAutoPreferDevice,
        .memoryRequiredFlags          = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        .hostMemoryMapping            = vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite
    };
}

auto MeshPool::makeVertexBufferPool(const MeshPoolConfig& cfg) -> BufferPool {
    BufferPoolConfig vertexBufferPoolCfg = bufferPoolBaseCfg(cfg);
    vertexBufferPoolCfg.bufferUsageFlags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    return BufferPool(vertexBufferPoolCfg);
}

auto MeshPool::makeIndexBufferPool(const MeshPoolConfig& cfg) -> BufferPool {
    BufferPoolConfig indexBufferPoolCfg = bufferPoolBaseCfg(cfg);
    indexBufferPoolCfg.bufferUsageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
    return BufferPool(indexBufferPoolCfg);
}

} // namespace projnekomata