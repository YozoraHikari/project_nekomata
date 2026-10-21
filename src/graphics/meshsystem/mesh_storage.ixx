export module projnekomata:graphics.meshsystem.mesh_storage;
import std;
import :graphics.vulkan.vk_buffer;

export namespace projnekomata::gfx {

struct MegabufferAllocation {
    vma::raii::VirtualAllocation virtualAllocation = nullptr;
    u64 size;
    u64 byteOffset;
};

class Megabuffer {
public:
    Megabuffer(std::nullptr_t) {}

    static auto create(u64 initialSize, u64 maxSize, u64 allocationUnitByteSize, vk::BufferUsageFlags usage) -> Megabuffer;

    auto allocate(u64 len) -> MegabufferAllocation;

    auto underlyingBuffer() -> vkrhi::VulkanBuffer& { return m_buffer; }

private:
    Megabuffer(vkrhi::VulkanBuffer&& buffer, vma::raii::VirtualBlock&& virtualBlock, u64 maxSize, u64 allocationUnitByteSize)
        : m_buffer(std::move(buffer)), m_virtualBlock(std::move(virtualBlock)), m_maxSize(maxSize), m_allocationUnitByteSize(allocationUnitByteSize) {}

    vkrhi::VulkanBuffer m_buffer = nullptr;
    vma::raii::VirtualBlock m_virtualBlock = nullptr;
    u64 m_maxSize = 0_u64;
    u64 m_allocationUnitByteSize = 0_u64;

    static auto createBufferWithSize(u64 size, vk::BufferUsageFlags usage) -> vkrhi::VulkanBuffer;
};

struct MeshStorageAllocation {
    MegabufferAllocation vertexPositions;
    MegabufferAllocation vertexAttributes;
    MegabufferAllocation indices;
};

class MeshStorage {
public:
    MeshStorage(std::nullptr_t);

    static auto create() -> MeshStorage;

    auto allocateSpaceForMesh(u64 vertexPositionsCount, u64 vertexAttributesBufferSize, u64 indexBufferSize) -> MeshStorageAllocation;

    auto positionsVertexBuffer() -> Megabuffer& { return m_positionsVertexBuffer; }
    auto attributesVertexBuffer() -> Megabuffer& { return m_attributesVertexBuffer; }
    auto indexBuffer() -> Megabuffer& { return m_indexBuffer; }

private:
    MeshStorage(Megabuffer&& positionsVertexBuffer, Megabuffer&& attributesVertexBuffer, Megabuffer&& indexBuffer)
        : m_positionsVertexBuffer(std::move(positionsVertexBuffer)), m_attributesVertexBuffer(std::move(attributesVertexBuffer)), m_indexBuffer(std::move(indexBuffer)) {}

    Megabuffer m_positionsVertexBuffer = nullptr;
    Megabuffer m_attributesVertexBuffer = nullptr;
    Megabuffer m_indexBuffer = nullptr;
};

}
