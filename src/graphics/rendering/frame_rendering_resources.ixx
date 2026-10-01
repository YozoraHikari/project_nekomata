export module projnekomata:graphics.rendering.frame_rendering_resources;
import std;
import projnekomata.corelib;
import :graphics.vulkan.vk_commands;
import :graphics.vulkan.vk_buffer;
import :graphics.vulkan.sync_primitives.fence;
import :graphics.vulkan.sync_primitives.binary_semaphore;
import :core.runtime.shared_data;
import :core.ecs.world.camera;
import :core.ecs.world.transform;
import :graphics.vulkan.vk_query_pool;
import :graphics.rendering.shared_rendering_resources;

export namespace projnekomata::gfx {

struct Transforms {
    Matrix4x4f model;
    Matrix4x4f prevModel;
    Matrix3x3f normalMatrix;
};

struct PointlightData {
    Vector3f position;
    Vector3f lightRadiance;
    float inverseRangeSq;

    u32 shadowmapAtlasImageSrtIndices[6];
    u32 shadowmapDescriptorIndexFirst;
};

struct SpotlightData {
    Vector3f position;
    Vector3f lightRadiance;
    Vector3f direction;

    float cosOuterAngle;

    float inverseRangeSq;
    float inverseAngleRange;

    u32 shadowmapAtlasImageSrtIndex;
    u32 shadowmapDescriptorIndex;
};

struct ShadowmapDescriptor {
    Matrix4x4f lightProjview;
    Vector2f atlasOffset;
    Vector2f atlasRectSize;
};

struct LodLevelInfo {
    u32 level = kLodListMaxLodCount - 1;
};

/// Per-frame rendering resources used exclusively by each frame and only by that frame.
///
/// # Access Contingency
/// | CPU Read/Write | GPU Access Scope across Queue | GPU Visibility Scope across Queue |
/// |----------------|-------------------------------|-----------------------------------|
/// | Yes            | Exclusive                     | Exclusive                         |
///
class FrameContextData {
public:
    FrameContextData(std::nullptr_t);
    FrameContextData(u32 initialMaxObjects);


    auto commandPool() -> vkrhi::VulkanCommandPool& { return m_commandPool; }
    auto commandBuffer() -> vkrhi::VulkanCommandBuffer& { return m_commandBuffer; }

    auto getLodLevel(usize renderableSparseIndex) -> u32 { return m_entityMeshLodLevels[renderableSparseIndex].level; }
    auto transformsBuffer() -> vkrhi::VulkanBuffer& { return m_transformsBuffer; }
    auto globalDataBuffer() -> vkrhi::VulkanBuffer& { return m_globalDataBuffer; }
    auto pointlightsBuffer() -> vkrhi::VulkanBuffer& { return m_pointlightsBuffer; }
    auto spotlightsBuffer() -> vkrhi::VulkanBuffer& { return m_spotlightsBuffer; }
    auto shadowmapDescriptorBuffer() -> vkrhi::VulkanBuffer& { return m_shadowMappingDescriptorBuffer; }

    auto pointlightsCount() const -> usize { return m_currPointlightCount; }
    auto spotlightsCount() const -> usize { return m_currSpotlightCount; }

    auto textureToSrtImageIDBuffer() -> vkrhi::VulkanBuffer& { return m_textureToSrtImageIDBuffer; }
    auto textureToSrtSamplerIDBuffer() -> vkrhi::VulkanBuffer& { return m_textureToSrtSamplerIDBuffer; }
    auto materialPropBuffer(usize structSize) -> vkrhi::VulkanBuffer& { return m_materialPropBuffersBySize[structSize]; }

    auto frameDoneFence() -> vkrhi::VulkanFence& { return m_frameDoneFence; }
    auto imageAcquiredSemaphore() -> vkrhi::VulkanBinarySemaphore& { return m_imageAcquiredSemaphore; }

    auto prepareBuffers(MRThreadsSharedDataLeaf& renderingData, SharedRenderingData& sharedRendResources, CameraComponent camera, const WorldTransformComponent& cameraTransform, float renderAspectRatio, float cameraPerspFocalLength, u64 frameIndex) -> void;

private:
    auto computeLodLevels(MRThreadsSharedDataLeaf& renderingData, Vector3f cameraPos, float cameraPerspFocalLength) -> void;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Commands
    vkrhi::VulkanCommandPool m_commandPool = nullptr;
    vkrhi::VulkanCommandBuffer m_commandBuffer = nullptr;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Object Buffers
    Vec<LodLevelInfo> m_entityMeshLodLevels = Vec<LodLevelInfo>::create();
    vkrhi::VulkanBuffer m_globalDataBuffer = nullptr;
    vkrhi::VulkanBuffer m_transformsBuffer = nullptr;
    vkrhi::VulkanBuffer m_pointlightsBuffer = nullptr;
    vkrhi::VulkanBuffer m_spotlightsBuffer = nullptr;
    usize m_currPointlightCount = 0;
    usize m_currSpotlightCount = 0;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Shadow Mapping Data
    vkrhi::VulkanBuffer m_shadowMappingDescriptorBuffer = nullptr;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Bindless Images Array
    vkrhi::VulkanBuffer m_textureToSrtImageIDBuffer = nullptr;
    vkrhi::VulkanBuffer m_textureToSrtSamplerIDBuffer = nullptr;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Material Buffers
    HashMap<usize, vkrhi::VulkanBuffer> m_materialPropBuffersBySize = HashMap<usize, vkrhi::VulkanBuffer>::create();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Synchronization

    /// Signaled after the frame has completed rendering
    vkrhi::VulkanFence m_frameDoneFence = nullptr;

    /// Signaled after a successful swapchain image acquire
    vkrhi::VulkanBinarySemaphore m_imageAcquiredSemaphore = nullptr;
};

}