export module projnekomata:core.containers.abia;
import std;
import projnekomata.corelib;

export namespace projnekomata {

class AtomicBitmapIndexAllocator {
public:
    AtomicBitmapIndexAllocator(std::nullptr_t) : m_bitmapSize(0), m_qwordCount(0) {}
    explicit AtomicBitmapIndexAllocator(usize bitmapSize)
        : m_bitmapSize(bitmapSize), m_qwordCount((bitmapSize + 63) / 64), m_bitmap(Vec<std::atomic<u64>>::filledWith(m_qwordCount, 0)) {
        for (auto& qword : m_bitmap) {
            qword.store(0, std::memory_order_relaxed);
        }
    }

    auto allocate() -> Option<usize> {
        // First, find a qword that has a free bit.
        for (usize i = 0; i < m_qwordCount; i++) {
            auto& qword = m_bitmap[i];
            u64 qwordBits = qword.load(std::memory_order_relaxed);

            while (qwordBits != kBitmapFullIndex) {
                // CAS loop: find first zero bit, set it, try to update.
                u64 freeBitfield = ~qwordBits;
                u32 bitIndex = __builtin_ctzll(freeBitfield);
                usize candidateId = i * 64 + bitIndex;
                if (candidateId >= m_bitmapSize) return None;

                u64 newQwordBits = qwordBits | (1ull << bitIndex);

                if (qword.compare_exchange_weak(qwordBits, newQwordBits,
                std::memory_order_acquire, std::memory_order_relaxed)) {
                    return Some(candidateId);
                }
                // If the condition above fails, then CAS failed (another thread just allocated and set a bit in this qword), must retry
            }
        }
        // The allocator is full at this point.
        return None;
    }

    auto release(usize index) -> void {
        auto qwordIndex = index / 64;
        auto bitIndex = index % 64;

        u64 mask = ~(1ull << bitIndex);
        m_bitmap[qwordIndex].fetch_and(mask, std::memory_order_release);
    }

    auto isIndexAllocated(usize index) const -> bool {
        auto qwordIndex = index / 64;
        auto bitIndex = index % 64;

        return (m_bitmap[qwordIndex].load(std::memory_order_acquire) & (1ull << bitIndex)) != 0;
    }

private:
    const usize m_bitmapSize;
    const usize m_qwordCount;
    Vec<std::atomic<u64>> m_bitmap;

    constexpr static u64 kBitmapFullIndex = ~0ull;
};

}