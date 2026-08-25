export module projnekomata:core.containers.freelist_pool;
import std;
import projnekomata.corelib;

// TODO: respect T/runtime object alignof

struct TaggedIndex {
    u64 indexAndTag;

    constexpr static auto create(u32 index, u32 tag) -> TaggedIndex {
        return TaggedIndex{static_cast<u64>(index) | (static_cast<u64>(tag) << 32)};
    }
    constexpr auto index() const -> u32 {
        return static_cast<u32>(indexAndTag & 0x00000000ffffffff);
    }
    constexpr auto tag() const -> u32 {
        return static_cast<u32>(indexAndTag >> 32);
    }
};

export namespace projnekomata {

template <typename T, u32 MaxElements, usize ChunkSizeHint = 65536> class VaSlotmap {
public:
    VaSlotmap(std::nullptr_t) noexcept {}
    ~VaSlotmap() {
        if (m_storage) Mem::vmDestroy(m_storage, MaxElements);
        debug_assert(m_aliveCount.load(std::memory_order_acquire) == 0, "some of the allocated objects in a VaSlotmap were not freed before its destruction");
    }

    static auto create() -> VaSlotmap {
        auto storage = Mem::vmReserve<Node>(MaxElements);
        return VaSlotmap(storage);
    }

    auto allocate() -> u32 {
        m_aliveCount.fetch_add(1, std::memory_order_relaxed);
        u32 index = popFreeIndex();
        if (index != kInvalidIndex) return index;

        index = m_allocatedCount.fetch_add(1, std::memory_order_relaxed);
        ensureCapacityForNewElem(index);
        return index;
    }

    auto free(u32 index) -> void {
        m_aliveCount.fetch_sub(1, std::memory_order_relaxed);
        load(index).resource.~T();
        pushFreeIndex(index);
    }

    template <typename... Args> auto emplace(Args&&... args) -> u32 {
        u32 index = allocate();
        new (&load(index).resource) T(std::forward<Args>(args)...);
        return index;
    }

    auto operator[](u32 index) -> T& { return load(index).resource; }
    auto operator[](u32 index) const -> const T& { return load(index).resource; }

    auto aliveCount() const -> u32 { return m_aliveCount.load(std::memory_order_acquire); }
    auto allocatedCount() const -> u32 { return m_allocatedCount.load(std::memory_order_acquire); }

private:
    union Node {
        T resource;
        u32 nextFree;
    };
    VaSlotmap(Node* storage) : m_storage(storage) {}

    static constexpr auto kChunkSize = std::max(ChunkSizeHint, std::bit_ceil(sizeof(Node)));
    static constexpr auto kVaSize = sizeof(Node) * MaxElements;
    static constexpr auto kInvalidIndex = ~0_u32;

    Node* m_storage = nullptr;
    std::atomic<u32> m_allocatedCount = 0;
    std::atomic<u32> m_aliveCount = 0;
    std::atomic<TaggedIndex> m_freelistHead = TaggedIndex::create(kInvalidIndex, 0);
    std::atomic<usize> m_storageCommittedBytes = 0;
    std::mutex m_allocationMutex;

    // ---- Access ---------------------------------------------------------------------------------------------------------------------------------------------

    auto load(u32 index) -> Node& { return m_storage[index]; }
    auto load(u32 index) const -> const Node& { return m_storage[index]; }

    // ---- Freelist Allocation --------------------------------------------------------------------------------------------------------------------------------

    auto pushFreeIndex(u32 index) -> void {
        auto head = m_freelistHead.load(std::memory_order_acquire);
        while (true) {
            load(index).nextFree = head.index();
            auto next = TaggedIndex::create(index, head.tag() + 1);
            if (m_freelistHead.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_acquire)) return;
        }
    }

    auto popFreeIndex() -> u32 {
        auto head = m_freelistHead.load(std::memory_order_acquire);
        while (true) {
            if (head.index() == kInvalidIndex) return kInvalidIndex;

            u32 nextIdx = load(head.index()).nextFree;
            auto next = TaggedIndex::create(nextIdx, head.tag() + 1);

            if (m_freelistHead.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_acquire)) return head.index();
        }
    }

    // ---- Bump Allocation ------------------------------------------------------------------------------------------------------------------------------------

    auto ensureCapacityForNewElem(u32 elemIndex) -> void {
        debug_assert(elemIndex < MaxElements, "VaSlotmap exceeded max capacity");
        usize elemIndexUsize = static_cast<usize>(elemIndex);
        usize spaceNeeded = (elemIndexUsize + 1) * sizeof(Node);
        usize spaceCurr = m_storageCommittedBytes.load(std::memory_order_acquire);

        if (spaceNeeded <= spaceCurr) return;

        std::scoped_lock lock(m_allocationMutex);
        // another thread might have committed a new page while waiting on the lock:
        spaceCurr = m_storageCommittedBytes.load(std::memory_order_acquire);
        if (spaceNeeded <= spaceCurr) return;

        u8* endPtr = reinterpret_cast<u8*>(m_storage) + spaceCurr;

        // make sure we don't commit past the end of the reserved VA:
        usize bytesLeft = kVaSize - spaceCurr;
        usize bytesToCommit = std::min(bytesLeft, kChunkSize);

        Mem::vmCommit(endPtr, bytesToCommit);

        m_storageCommittedBytes.store(spaceCurr + bytesToCommit, std::memory_order_release);
    }
};

template <u32 MaxElements, usize ChunkSizeHint = 65536> class VaSlotmapNoType {
public:
    VaSlotmapNoType(std::nullptr_t) noexcept {}
    VaSlotmapNoType(u8* storage, usize elemSize) : m_elemSize(elemSize), m_storage(storage) {}
    ~VaSlotmapNoType() {
        if (m_storage) Mem::vmDestroy(m_storage, MaxElements);
        debug_assert(m_aliveCount.load(std::memory_order_acquire) == 0, "some of the allocated objects in a VaSlotmapNoType were not freed before its destruction");
    }

    VaSlotmapNoType(const VaSlotmapNoType&) = delete;
    VaSlotmapNoType& operator=(const VaSlotmapNoType&) = delete;
    VaSlotmapNoType(VaSlotmapNoType&&) = delete;
    VaSlotmapNoType& operator=(VaSlotmapNoType&&) = delete;

    static auto create(usize elemSize) -> VaSlotmapNoType {
        auto storage = Mem::vmReserve<u8>(MaxElements * elemSize);
        return VaSlotmapNoType(storage, elemSize);
    }

    static auto createUnique(usize elemSize) -> Unique<VaSlotmapNoType> {
        auto storage = Mem::vmReserve<u8>(MaxElements * elemSize);
        auto uniq = Unique<VaSlotmapNoType>::create(storage, elemSize);
        return uniq;
    }

    auto allocate() -> u32 {
        m_aliveCount.fetch_add(1, std::memory_order_relaxed);
        u32 index = popFreeIndex();
        if (index != kInvalidIndex) return index;

        index = m_allocatedCount.fetch_add(1, std::memory_order_relaxed);
        ensureCapacityForNewElem(index);
        return index;
    }

    template <typename T> auto free(u32 index) -> void {
        m_aliveCount.fetch_sub(1, std::memory_order_relaxed);
        load<T>(index).~T();
        pushFreeIndex(index);
    }

    template <typename T, typename... Args> auto emplace(Args&&... args) -> u32 {
        debug_assert(sizeof(T) <= nodeSize(), "emplaced element size is larger than the node size");
        u32 index = allocate();
        new (&load<T>(index)) T(std::forward<Args>(args)...);
        return index;
    }

    template <typename T> auto load(u32 index) -> T& {
        debug_assert(sizeof(T) <= nodeSize(), "accessed element size is larger than the node size");
        return *reinterpret_cast<T*>(m_storage + index * nodeSize());
    }
    template <typename T> auto load(u32 index) const -> const T& {
        debug_assert(sizeof(T) <= nodeSize(), "accessed element size is larger than the node size");
        return *reinterpret_cast<const T*>(m_storage + index * nodeSize());
    }

    auto aliveCount() const -> u32 { return m_aliveCount.load(std::memory_order_acquire); }
    auto allocatedCount() const -> u32 { return m_allocatedCount.load(std::memory_order_acquire); }
    auto stride() const -> usize { return nodeSize(); }
    auto data() const -> Slice<const u8> { return Slice<const u8>(m_storage, allocatedCount() * nodeSize()); }

private:

    static constexpr auto kInvalidIndex = ~0_u32;

    usize m_elemSize;
    constexpr auto nodeSize() const -> usize { return std::max(m_elemSize, sizeof(u32)); }
    constexpr auto vaSize() const -> usize { return MaxElements * m_elemSize; }
    constexpr auto chunkSize() const -> usize { return std::max(ChunkSizeHint, std::bit_ceil(m_elemSize)); }

    u8* m_storage = nullptr;
    std::atomic<u32> m_allocatedCount = 0;
    std::atomic<u32> m_aliveCount = 0;
    std::atomic<TaggedIndex> m_freelistHead = TaggedIndex::create(kInvalidIndex, 0);
    std::atomic<usize> m_storageCommittedBytes = 0;
    std::mutex m_allocationMutex;

    // ---- Access ---------------------------------------------------------------------------------------------------------------------------------------------

    auto pushFreeIndex(u32 index) -> void {
        auto head = m_freelistHead.load(std::memory_order_acquire);
        while (true) {
            load<u32>(index) = head.index();
            auto next = TaggedIndex::create(index, head.tag() + 1);
            if (m_freelistHead.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_acquire)) return;
        }
    }

    auto popFreeIndex() -> u32 {
        auto head = m_freelistHead.load(std::memory_order_acquire);
        while (true) {
            if (head.index() == kInvalidIndex) return kInvalidIndex;

            u32 nextIdx = load<u32>(head.index());
            auto next = TaggedIndex::create(nextIdx, head.tag() + 1);

            if (m_freelistHead.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_acquire)) return head.index();
        }
    }

    // ---- Bump Allocation ------------------------------------------------------------------------------------------------------------------------------------

    auto ensureCapacityForNewElem(u32 elemIndex) -> void {
        debug_assert(elemIndex < MaxElements, "VaSlotmap exceeded max capacity");
        usize elemIndexUsize = static_cast<usize>(elemIndex);
        usize spaceNeeded = (elemIndexUsize + 1) * nodeSize();
        usize spaceCurr = m_storageCommittedBytes.load(std::memory_order_acquire);

        if (spaceNeeded <= spaceCurr) return;

        std::scoped_lock lock(m_allocationMutex);
        // another thread might have committed a new page while waiting on the lock:
        spaceCurr = m_storageCommittedBytes.load(std::memory_order_acquire);
        if (spaceNeeded <= spaceCurr) return;

        u8* endPtr = m_storage + spaceCurr;

        // make sure we don't commit past the end of the reserved VA:
        usize bytesLeft = vaSize() - spaceCurr;
        usize bytesToCommit = std::min(bytesLeft, chunkSize());

        Mem::vmCommit(endPtr, bytesToCommit);

        m_storageCommittedBytes.store(spaceCurr + bytesToCommit, std::memory_order_release);
    }
};


}