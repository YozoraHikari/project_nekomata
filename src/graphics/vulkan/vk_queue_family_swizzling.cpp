module projnekomata;
import :graphics.vulkan.vk_queue_family_swizzling;

namespace projnekomata::gfx::vkrhi {

VulkanQueueFamilySwizzling::VulkanQueueFamilySwizzling(std::nullptr_t) {  }
VulkanQueueFamilySwizzling::VulkanQueueFamilySwizzling(u32 graphicsQueueFamilyIndex, u32 presentQueueFamilyIndex, u32 asyncComputeQueueFamilyIndex) {
    std::array<u32, 3> searchPattern = {};

    for (u32 i = 1; i < 8; i++) {
        u32 nextFreePos = 0;

        auto tryAdd = [&](u32 index) {
            // Make sure it's not in the array first.
            for (u32 j = 0; j < nextFreePos; j++)
                if (searchPattern[j] == index) return;

            searchPattern[nextFreePos] = index;
            nextFreePos++;
        };

        if (i & 1 << 0)
            tryAdd(graphicsQueueFamilyIndex);

        if (i & 1 << 1)
            tryAdd(presentQueueFamilyIndex);

        if (i & 1 << 2)
            tryAdd(asyncComputeQueueFamilyIndex);

        u32 searchPatternSize = nextFreePos;

        auto query = std::search(
            m_queueFamilyIndexPermutationTable.begin(),
            m_queueFamilyIndexPermutationTable.begin() + m_queueFamilyIndexPermutationTableSize,
            searchPattern.begin(),
            searchPattern.begin() + searchPatternSize
            );

        if (query != m_queueFamilyIndexPermutationTable.begin() + m_queueFamilyIndexPermutationTableSize) {
            m_queueFamilyIndexPermutationViews[i] = Slice<const u32>(query, searchPatternSize);
            continue;
        }

        if (m_queueFamilyIndexPermutationTableSize + searchPatternSize >= m_queueFamilyIndexPermutationTable.size()) {
            panic("ran out of space for queue family index permutations");
        }

        u32* copyDst = m_queueFamilyIndexPermutationTable.data() + m_queueFamilyIndexPermutationTableSize;
        m_queueFamilyIndexPermutationViews[i] = Slice<const u32>(copyDst, searchPatternSize);
        std::copy_n(searchPattern.begin(), searchPatternSize, copyDst);
        m_queueFamilyIndexPermutationTableSize += searchPatternSize;
    }
}

} // namespace projnekomata::gfx::vkrhi