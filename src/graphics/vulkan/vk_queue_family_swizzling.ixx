export module projnekomata:graphics.vulkan.vk_queue_family_swizzling;
import std;
import projnekomata.cs;

export namespace projnekomata::gfx::vkrhi {

enum class QueueFamily : u8 {
    Graphics     = 1 << 0,
    Present      = 1 << 1,
    AsyncCompute = 1 << 2,
};
constexpr QueueFamily operator|(QueueFamily a, QueueFamily b) {
    return static_cast<QueueFamily>(static_cast<u8>(a) | static_cast<u8>(b));
}

class VulkanQueueFamilySwizzling {
public:
    VulkanQueueFamilySwizzling(std::nullptr_t);
    VulkanQueueFamilySwizzling(u32 graphicsQueueFamilyIndex, u32 presentQueueFamilyIndex, u32 asyncComputeQueueFamilyIndex);

    VulkanQueueFamilySwizzling(VulkanQueueFamilySwizzling&& other) noexcept
        : m_queueFamilyIndexPermutationTable(other.m_queueFamilyIndexPermutationTable)
        , m_queueFamilyIndexPermutationTableSize(other.m_queueFamilyIndexPermutationTableSize)
        , m_queueFamilyIndexPermutationViews(other.m_queueFamilyIndexPermutationViews)
    {
        correctSpans(other);
    }

    VulkanQueueFamilySwizzling& operator=(VulkanQueueFamilySwizzling&& other) noexcept {
        m_queueFamilyIndexPermutationTable     = other.m_queueFamilyIndexPermutationTable;
        m_queueFamilyIndexPermutationTableSize = other.m_queueFamilyIndexPermutationTableSize;
        m_queueFamilyIndexPermutationViews     = other.m_queueFamilyIndexPermutationViews;
        correctSpans(other);
        return *this;
    }

    VulkanQueueFamilySwizzling(const VulkanQueueFamilySwizzling& other) noexcept
        : m_queueFamilyIndexPermutationTable(other.m_queueFamilyIndexPermutationTable)
        , m_queueFamilyIndexPermutationTableSize(other.m_queueFamilyIndexPermutationTableSize)
        , m_queueFamilyIndexPermutationViews(other.m_queueFamilyIndexPermutationViews) {
        const u32* oldBase = other.m_queueFamilyIndexPermutationTable.data();
        const u32* newBase = m_queueFamilyIndexPermutationTable.data();
        for (auto& span : m_queueFamilyIndexPermutationViews)
            if (!span.isEmpty())
                span = Slice<const u32>(newBase + (span.data() - oldBase), span.len());
    }

    // TODO: This is horrible AF. Fix the physical device props struct to be properly initialized instead later
    VulkanQueueFamilySwizzling& operator=(const VulkanQueueFamilySwizzling& other) noexcept {
        if (this == &other) return *this;

        const u32* oldBase = other.m_queueFamilyIndexPermutationTable.data();

        m_queueFamilyIndexPermutationTable     = other.m_queueFamilyIndexPermutationTable;
        m_queueFamilyIndexPermutationTableSize = other.m_queueFamilyIndexPermutationTableSize;
        m_queueFamilyIndexPermutationViews     = other.m_queueFamilyIndexPermutationViews;

        const u32* newBase = m_queueFamilyIndexPermutationTable.data();
        for (auto& span : m_queueFamilyIndexPermutationViews)
            if (!span.isEmpty())
                span = Slice<const u32>(newBase + (span.data() - oldBase), span.len());

        return *this;
    }

    auto allUniqueQueueIndices() const -> Slice<const u32> {
        return (*this)[QueueFamily::Graphics | QueueFamily::Present | QueueFamily::AsyncCompute];
    }

    auto operator[](QueueFamily queueFamily) const -> Slice<const u32> {
        return m_queueFamilyIndexPermutationViews[static_cast<u8>(queueFamily)];
    }

private:
    std::array<u32, 5> m_queueFamilyIndexPermutationTable = {};
    u32 m_queueFamilyIndexPermutationTableSize = 0;
    std::array<Slice<const u32>, 8> m_queueFamilyIndexPermutationViews = {};

    void correctSpans(const VulkanQueueFamilySwizzling& other) noexcept {
        const u32* oldBase = other.m_queueFamilyIndexPermutationTable.data();
        const u32* newBase = m_queueFamilyIndexPermutationTable.data();

        for (auto& span : m_queueFamilyIndexPermutationViews)
            if (!span.isEmpty())
                span = Slice<const u32>(newBase + (span.data() - oldBase), span.len());
    }
};

}