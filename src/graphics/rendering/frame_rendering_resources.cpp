module;
#include <string.h>
module projnekomata;
import vulkan;
import fmt;
import vk_mem_alloc;
import :graphics.vulkan.context;
import :graphics.vulkan.vk_queue_family_swizzling;
import :graphics.rendering.frame_rendering_resources;
import :graphics.shaders.globallayout;

namespace projnekomata::gfx {

FrameRenderingResources::FrameRenderingResources(std::nullptr_t) {  }

FrameRenderingResources::FrameRenderingResources(u32 initialMaxObjects) {
    m_commandPool = vkrhi::VulkanCommandPool::createForGraphics(true);
    m_commandBuffer = m_commandPool.allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);

    m_transformsBuffer = vkrhi::VulkanBuffer::builder()
        .name("Frame Transforms")
        .len(initialMaxObjects * sizeof(Transforms))
        .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
        .build();

    m_globalDataBuffer = vkrhi::VulkanBuffer::builder()
        .name("Frame Shader Global Data")
        .len(sizeof(ShGlobalData))
        .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
        .build();

    m_pointlightsBuffer = vkrhi::VulkanBuffer::builder()
        .name("Frame Pointlights")
        .len(1024 * sizeof(PointlightData))
        .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
        .build();

    m_frameDoneFence = vkrhi::VulkanFence::create(true);
    m_imageAcquiredSemaphore = vkrhi::VulkanBinarySemaphore::create();
}

auto FrameRenderingResources::prepareBuffers(MRThreadsSharedDataLeaf& renderingData, SharedRenderingResources& sharedRendResources, CameraComponent camera, const WorldTransformComponent& cameraTransform, float renderAspectRatio, u64 frameIndex) -> void {
    auto projectionMatrix = camera.computeProjectionMatrix(renderAspectRatio);
    auto projInverse = projectionMatrix.inverse().unwrapOr(Matrix4x4f::identity());
    auto& cameraModelMatrix = cameraTransform.m_transform;
    auto viewMatrix = cameraModelMatrix.inverseRigid();
    auto viewportSize = Vector2f(renderingData.m_currentWindowExtent.width, renderingData.m_currentWindowExtent.height);

    // ---- Shader Resource Table Handles ----------------------------------------------------------------------------------------------------------------------

    auto srtImageHandlesData = renderingData.m_textureToImageShaderIndexSnapshot.asSlice();
    auto srtSamplerHandlesData = renderingData.m_textureToSamplerShaderIndexSnapshot.asSlice();
    if (m_textureToSrtImageIDBuffer.vkBuffer() == nullptr || m_textureToSrtImageIDBuffer.size() < srtImageHandlesData.len() * sizeof(u32)) {
        m_textureToSrtImageIDBuffer = vkrhi::VulkanBuffer::builder()
            .name("Texture to SRT Image ID Buffer")
            .len(srtImageHandlesData.len() * sizeof(u32))
            .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
            .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
            .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
            .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
            .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
            .build();
    }

    if (m_textureToSrtSamplerIDBuffer.vkBuffer() == nullptr || m_textureToSrtSamplerIDBuffer.size() < srtSamplerHandlesData.len() * sizeof(u32)) {
        m_textureToSrtSamplerIDBuffer = vkrhi::VulkanBuffer::builder()
            .name("Texture to SRT Sampler ID Buffer")
            .len(srtSamplerHandlesData.len() * sizeof(u32))
            .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
            .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
            .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
            .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
            .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
            .build();
    }

    memcpy(m_textureToSrtImageIDBuffer.memoryHostPtr(), srtImageHandlesData.data(), srtImageHandlesData.len() * sizeof(u32));
    memcpy(m_textureToSrtSamplerIDBuffer.memoryHostPtr(), srtSamplerHandlesData.data(), srtSamplerHandlesData.len() * sizeof(u32));

    // ---- Material Data --------------------------------------------------------------------------------------------------------------------------------------

    for (auto& [size, heapdata] : renderingData.m_materialHeapSnapshotsBySize.iter()) {
        // make sure we have an appropriate size buffer first:
        if (!m_materialPropBuffersBySize.contains(size)) {
            auto name = fmt::format("Material Prop Buffer, len: {}", size);
            auto buffer = vkrhi::VulkanBuffer::builder()
                .name(name)
                .len(heapdata.len())
                .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
                .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
                .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
                .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
                .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
                .build();

            m_materialPropBuffersBySize.insert(size, std::move(buffer));
        } else if (m_materialPropBuffersBySize[size].size() < heapdata.len()) {
            // if the buffer is too small, resize it:
            auto name = fmt::format("Material Prop Buffer, len: {}", size);
            m_materialPropBuffersBySize[size] = vkrhi::VulkanBuffer::builder()
                .name(name)
                .len(heapdata.len())
                .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
                .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
                .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
                .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
                .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
                .build();
        }

        memcpy(m_materialPropBuffersBySize[size].memoryHostPtr(), heapdata.data(), heapdata.len());
    }

    // ---- Per-Object Data ------------------------------------------------------------------------------------------------------------------------------------

    for (auto [i, rend] : renderingData.m_renderables.m_storage.iter().enumerate()) {
        auto entSparseIndex = renderingData.m_renderables.m_storageToEntity[i];

        auto modelMatrix = math::Matrix4x4f::identity();
        if (renderingData.m_transforms.containsEntity(entSparseIndex)) {
            modelMatrix = renderingData.m_transforms.get(entSparseIndex).m_transform;
        }

        auto normalMatrix = modelMatrix
            .submatrix<3, 3>(0, 0)
            .inverse()
            .map([](auto m) { return m.transpose(); })
            .unwrapOr(Matrix3x3f::identity());

        auto transforms = Transforms {
            .model = modelMatrix,
            .prevModel = sharedRendResources.getLastRenderableModelMatrix(entSparseIndex.index()),
            .normalMatrix = normalMatrix,
        };
        sharedRendResources.getLastRenderableModelMatrix(entSparseIndex.index()) = modelMatrix;

        memcpy(m_transformsBuffer.memoryHostPtr() + i * sizeof(Transforms), &transforms, sizeof(Transforms));
    }

    // ---- Pointlights ----------------------------------------------------------------------------------------------------------------------------------------

    for (auto [i, light] : renderingData.m_pointlights.m_storage.iter().enumerate()) {
        auto entSparseIndex = renderingData.m_pointlights.m_storageToEntity[i];

        auto position = Vector3f(0.0f);
        if (renderingData.m_transforms.containsEntity(entSparseIndex)) {
            auto& matrix = renderingData.m_transforms.get(entSparseIndex).m_transform;
            position = matrix.decomposePosition();
        }

        auto pointlightData = PointlightData {
            .position = position,
            .lightRadiance = light.lightRadiance
        };

        memcpy(m_pointlightsBuffer.memoryHostPtr() + i * sizeof(PointlightData), &pointlightData, sizeof(PointlightData));
    }

    // ---- Global Data ----------------------------------------------------------------------------------------------------------------------------------------

    static Vector2f smaat2xJitterPattern[2] = {
        Vector2f( 0.25f, -0.25f),
        Vector2f(-0.25f,  0.25f)
    };

    auto jitter = smaat2xJitterPattern[frameIndex % 2];
    auto jitterOffset = 2.0f * jitter.componentWiseDivide(viewportSize);

    auto jitteredProj = projectionMatrix;
    jitteredProj[0, 2] += jitterOffset.x();
    jitteredProj[1, 2] += jitterOffset.y();

    auto projview = projectionMatrix * viewMatrix;
    auto jitteredProjview = jitteredProj * viewMatrix;

    auto camModelMatrixNoTranslation = cameraModelMatrix;
    camModelMatrixNoTranslation[0, 3] = 0.0f;
    camModelMatrixNoTranslation[1, 3] = 0.0f;
    camModelMatrixNoTranslation[2, 3] = 0.0f;
    auto viewMatrixNoTranslation = camModelMatrixNoTranslation.inverseRigid();

    auto projviewNoTranslation = projectionMatrix * viewMatrixNoTranslation;
    auto projviewNoTranslationInverse = camModelMatrixNoTranslation * projInverse;

    auto cameraPos = cameraModelMatrix.decomposePosition();

    auto globdata = ShGlobalData {
        .jitteredProjview = jitteredProjview,
        .projview = projview,
        .prevProjview = sharedRendResources.m_lastProjview,
        .prevProjviewNoTranslation = sharedRendResources.m_lastProjviewNoTranslation,
        .projviewInverse = cameraModelMatrix * projInverse,
        .projviewNoTranslationInverse = projviewNoTranslationInverse,
        .cameraPos = cameraPos,
        .frameIndex = static_cast<u32>(frameIndex)
    };
    sharedRendResources.m_lastProjview = jitteredProjview;
    sharedRendResources.m_lastProjviewNoTranslation = projviewNoTranslation;

    memcpy(m_globalDataBuffer.memoryHostPtr(), &globdata, sizeof(ShGlobalData));
}

} // namespace projnekomata