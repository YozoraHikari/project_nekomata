module projnekomata;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_buffer;

namespace projnekomata::gfx::vkrhi {

VulkanBuffer::VulkanBuffer(std::nullptr_t) {}
VulkanBuffer::VulkanBuffer(vk::raii::Buffer&& buffer, vma::raii::Allocation&& allocation, u64 size, u8* mVkBufferMemoryHostPtr, vk::DeviceAddress mVkBufferMemoryDevicePtr) : m_vkBuffer(std::move(buffer)), m_vmaAllocation(std::move(allocation)), m_vkBufferMemoryHostPtr(mVkBufferMemoryHostPtr), m_vkBufferMemoryDevicePtr(mVkBufferMemoryDevicePtr), m_size(size) {}

auto VulkanBuffer::create(u64 size, vk::BufferUsageFlags usage, VulkanBufferMemoryMapping hostMemoryMapping, vma::MemoryUsage memoryUsage, vk::MemoryPropertyFlags memoryRequiredFlags, Slice<const u32> queueFamilyIndices) -> VulkanBuffer {
    auto bufferCreateInfo = vk::BufferCreateInfo{}
        .setSize(size)
        .setUsage(usage)
        .setQueueFamilyIndices(queueFamilyIndices)
        .setSharingMode(queueFamilyIndices.len() == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent);

    vma::AllocationCreateFlags mappedMemoryBit;

    switch (hostMemoryMapping) {
        case VulkanBufferMemoryMapping::MapForSequentialWrite: mappedMemoryBit |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite; break;
        case VulkanBufferMemoryMapping::MapForRandomAccess:    mappedMemoryBit |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessRandom; break;
        default: break;
    }

    auto allocationCreateInfo = vma::AllocationCreateInfo{}
        .setUsage(memoryUsage)
        .setRequiredFlags(memoryRequiredFlags)
        .setFlags(mappedMemoryBit);

    auto [allocation, buffer] = vkCheckResult(VulkanContext::get().vmaAllocator().createBuffer(bufferCreateInfo, allocationCreateInfo)).split();

    auto memoryDevicePtr = vk::DeviceAddress(nullptr);
    if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        auto bdaInfo = vk::BufferDeviceAddressInfo{}
            .setBuffer(buffer);

        memoryDevicePtr = VulkanContext::get().vkDevice().getBufferAddress(bdaInfo);
    }

    u8* memoryHostPtr = nullptr;
    if (mappedMemoryBit) {
        memoryHostPtr = static_cast<u8*>(allocation.getInfo().pMappedData);
    }


    return VulkanBuffer(std::move(buffer), std::move(allocation), size, memoryHostPtr, memoryDevicePtr);
}

}