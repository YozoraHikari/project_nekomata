export module projnekomata:graphics.vulkan.vk_buffer;
import std;
import projnekomata.corelib;
import vulkan;
import vk_mem_alloc;
import :graphics.vulkan.vk_gpu_obrm;
import :graphics.vulkan.context;
import :graphics.vulkan.dbg_object_names;

export namespace projnekomata::gfx::vkrhi {

enum class VulkanBufferMemoryMapping {
    DontMap,
    MapForSequentialWrite,
    MapForRandomAccess,
};

class VulkanBufferBuilder;
class VulkanBuffer {
public:
    VulkanBuffer(std::nullptr_t);
    VulkanBuffer(vk::raii::Buffer&& buffer, vma::raii::Allocation&& allocation, u64 size, u8* mVkBufferMemoryHostPtr, vk::DeviceAddress mVkBufferMemoryDevicePtr);

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&&) = default;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(VulkanBuffer&&)  noexcept = default;

    constexpr static auto builder() -> VulkanBufferBuilder;

    static auto create(u64 size, vk::BufferUsageFlags usage, VulkanBufferMemoryMapping hostMemoryMapping, vma::MemoryUsage memoryUsage, vk::MemoryPropertyFlags memoryRequiredFlags, Slice<const u32> queueFamilyIndices) -> VulkanBuffer;

    [[nodiscard]] auto vkBuffer() const        -> const vk::raii::Buffer& { return m_vkBuffer.vkHandle(); }
    [[nodiscard]] auto size() const            -> u64 { return m_size; }
    [[nodiscard]] auto memoryHostPtr() const   -> u8* { return m_vkBufferMemoryHostPtr; }
    [[nodiscard]] auto memoryDevicePtr() const -> vk::DeviceAddress { return m_vkBufferMemoryDevicePtr; }

private:
    VulkanAsyncRaiiWrapper<vk::raii::Buffer>      m_vkBuffer      = nullptr;
    VulkanAsyncRaiiWrapper<vma::raii::Allocation> m_vmaAllocation = nullptr;


    u8*               m_vkBufferMemoryHostPtr   = nullptr;
    vk::DeviceAddress m_vkBufferMemoryDevicePtr = vk::DeviceAddress(nullptr);

    u64 m_size{};
};

class VulkanBufferBuilder : public BuilderObjectNameMixin<VulkanBufferBuilder> {
public:
    auto len(u64 len) noexcept -> VulkanBufferBuilder& { m_bufferCreateInfo.setSize(len); return *this; }
    auto usage(vk::BufferUsageFlags usage) noexcept -> VulkanBufferBuilder& { m_bufferCreateInfo.setUsage(usage); return *this; }
    auto memoryMapping(VulkanBufferMemoryMapping mapping) noexcept -> VulkanBufferBuilder& { m_hostMemoryMapping = mapping; return *this; }
    auto memoryUsage(vma::MemoryUsage usage) noexcept -> VulkanBufferBuilder& { m_allocationCreateInfo.usage = usage; return *this; }
    auto memoryRequiredFlags(vk::MemoryPropertyFlags flags) noexcept -> VulkanBufferBuilder& { m_allocationCreateInfo.requiredFlags = flags; return *this; }
    auto queueFamilyIndices(Slice<const u32> indices) noexcept -> VulkanBufferBuilder& { m_bufferCreateInfo.setQueueFamilyIndices(indices); return *this; }
    auto queueFamilyIndices(QueueFamily families) noexcept -> VulkanBufferBuilder& { auto queues = VulkanContext::get().vkPhysicalDeviceProps().m_queueFamilies[families]; m_bufferCreateInfo.setQueueFamilyIndices(queues); return *this; }
    auto build() -> VulkanBuffer {
        m_bufferCreateInfo.sharingMode = m_bufferCreateInfo.queueFamilyIndexCount == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

        vma::AllocationCreateFlags mappedMemoryBit;

        switch (m_hostMemoryMapping) {
            case VulkanBufferMemoryMapping::MapForSequentialWrite: mappedMemoryBit |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite; break;
            case VulkanBufferMemoryMapping::MapForRandomAccess: mappedMemoryBit |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessRandom; break;
            default: break;
        }

        m_allocationCreateInfo.flags |= mappedMemoryBit;

        auto [allocation, buffer] = vkCheckResult(VulkanContext::get().vmaAllocator().createBuffer(m_bufferCreateInfo, m_allocationCreateInfo)).split();

        dbgApplyDebugName(*buffer);

        auto memoryDevicePtr = vk::DeviceAddress(nullptr);
        if (m_bufferCreateInfo.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
            auto bdaInfo = vk::BufferDeviceAddressInfo{}
                .setBuffer(buffer);

            memoryDevicePtr = VulkanContext::get().vkDevice().getBufferAddress(bdaInfo);
        }

        u8* memoryHostPtr = nullptr;
        if (mappedMemoryBit) {
            memoryHostPtr = static_cast<u8*>(allocation.getInfo().pMappedData);
        }

        return VulkanBuffer(std::move(buffer), std::move(allocation), m_bufferCreateInfo.size, memoryHostPtr, memoryDevicePtr);
    }

private:
    constexpr VulkanBufferBuilder() = default;

    vk::BufferCreateInfo m_bufferCreateInfo;
    vma::AllocationCreateInfo m_allocationCreateInfo;

    VulkanBufferMemoryMapping m_hostMemoryMapping = VulkanBufferMemoryMapping::DontMap;

    friend class VulkanBuffer;
};
constexpr auto VulkanBuffer::builder() -> VulkanBufferBuilder { return VulkanBufferBuilder(); }

}
