module projnekomata;
import :graphics.meshsystem.mesh_storage;

namespace projnekomata::gfx {

auto Megabuffer::create(u64 initialSize, u64 maxSize, u64 allocationUnitByteSize, vk::BufferUsageFlags usage) -> Megabuffer {
    auto buffer = createBufferWithSize(initialSize * allocationUnitByteSize, usage);

    auto virtualBlockCreateInfo = vma::VirtualBlockCreateInfo{}
        .setSize(maxSize);

    auto virtualBlock = vkrhi::vkCheckResult(vma::raii::createVirtualBlock(virtualBlockCreateInfo));

    return Megabuffer(std::move(buffer), std::move(virtualBlock), maxSize, allocationUnitByteSize);
}

auto Megabuffer::allocate(u64 len) -> MegabufferAllocation {
    auto virtualAllocationCreateInfo = vma::VirtualAllocationCreateInfo{}
        .setSize(len);
    auto virtualAllocation = vkrhi::vkCheckResult(m_virtualBlock.allocate(virtualAllocationCreateInfo));
    auto vaInfo = virtualAllocation.getInfo();

    auto needsResize = (vaInfo.size + vaInfo.offset) * m_allocationUnitByteSize > m_buffer.size();
    if (needsResize) panic("unimplemented"); // TODO: implement once the Vulkan handle RAII situation is sorted out

    return { .virtualAllocation = std::move(virtualAllocation), .size = len * m_allocationUnitByteSize, .byteOffset = vaInfo.offset * m_allocationUnitByteSize };
}

auto Megabuffer::createBufferWithSize(u64 size, vk::BufferUsageFlags usage) -> vkrhi::VulkanBuffer {
    return vkrhi::VulkanBuffer::builder()
        .len(size)
        .usage(usage)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
        .build();
}

auto MeshStorage::create() -> MeshStorage {
    constexpr u64 expectedVertexCount = 500000;
    constexpr u64 expectedIndexCount = 1000000;
    constexpr u64 expectedAvgVertexAttribSize = 36;

    auto vertexPositions = Megabuffer::create(expectedVertexCount * sizeof(Vector3f), 1024 * 1024 * 1024, sizeof(Vector3f),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
    auto vertexAttribs = Megabuffer::create(expectedVertexCount * expectedAvgVertexAttribSize, 1024 * 1024 * 1024, 1,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
    auto indices = Megabuffer::create(expectedIndexCount * sizeof(u32), 1024 * 1024 * 1024, 1,
        vk::BufferUsageFlagBits::eIndexBuffer);

    return MeshStorage(std::move(vertexPositions), std::move(vertexAttribs), std::move(indices));
}

auto MeshStorage::allocateSpaceForMesh(u64 vertexPositionsCount, u64 vertexAttributesBufferSize, u64 indexBufferSize) -> MeshStorageAllocation {
    auto vertexPositions = m_positionsVertexBuffer.allocate(vertexPositionsCount);
    auto vertexAttribs = m_attributesVertexBuffer.allocate(vertexAttributesBufferSize);
    auto indices = m_indexBuffer.allocate(indexBufferSize);

    return MeshStorageAllocation {
        .vertexPositions = std::move(vertexPositions),
        .vertexAttributes = std::move(vertexAttribs),
        .indices = std::move(indices)
    };
}
}
