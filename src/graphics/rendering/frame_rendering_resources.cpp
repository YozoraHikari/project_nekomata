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

FrameContextData::FrameContextData(std::nullptr_t) {  }

FrameContextData::FrameContextData(u32 initialMaxObjects) {
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

    m_spotlightsBuffer = vkrhi::VulkanBuffer::builder()
        .name("Frame Spotlights")
        .len(1024 * sizeof(SpotlightData))
        .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
        .build();

    m_shadowMappingDescriptorBuffer = vkrhi::VulkanBuffer::builder()
        .name("Frame Shadowmap Descriptors")
        .len(1024 * sizeof(ShadowmapDescriptor))
        .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
        .build();

    m_frameDoneFence = vkrhi::VulkanFence::create(true);
    m_imageAcquiredSemaphore = vkrhi::VulkanBinarySemaphore::create();
}

auto FrameContextData::prepareBuffers(MRThreadsSharedDataLeaf& renderingData, SharedRenderingData& sharedRendResources, CameraComponent camera, const WorldTransformComponent& cameraTransform, float renderAspectRatio, float cameraPerspFocalLength, u64 frameIndex) -> void {
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
        if (m_transformsBuffer.size() / sizeof(Transforms) <= i) {
            panic("not enough space in transforms buffer");
        }
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

    m_currPointlightCount = 0;
    m_currSpotlightCount = 0;
    sharedRendResources.resetShadowmapAtlases();

    for (auto [i, light] : renderingData.m_lights.m_storage.iter().enumerate()) {
        auto entSparseIndex = renderingData.m_lights.m_storageToEntity[i];

        auto position = Vector3f(0.0f);
        auto rotationDir = Vector3f(0.0f);
        auto matrix = Matrix4x4f::identity();
        if (renderingData.m_transforms.containsEntity(entSparseIndex)) {
            matrix = renderingData.m_transforms.get(entSparseIndex).m_transform;
            position = matrix.decomposePosition();
            rotationDir = -Vector3f(matrix[0, 2], matrix[1, 2], matrix[2, 2]).normalize();
        }

        auto invRangeSq = 1.0f / (light.range * light.range);

        switch (light.type) {
            case LightType::Point: {
                always_assert(m_currPointlightCount < m_pointlightsBuffer.size() / sizeof(PointlightData), "not enough space in pointlights buffer");

                u32 shadowmapAtlasImageSrtIndices[6] = { ~0u };
                u32 shadowmapDescriptorIndexFirst = 0;

                if (light.castShadows) {
                    always_assert(sharedRendResources.m_currShadowmapDescriptorCount + 6 <= m_shadowMappingDescriptorBuffer.size() / sizeof(ShadowmapDescriptor), "not enough space in shadowmap descriptor buffer");

                    auto shadowmapFaceRectSize = light.shadowMapFaceSize;
                    auto shadowMapFaceSizef = Vector2f(light.shadowMapFaceSize.x(), light.shadowMapFaceSize.y());
                    auto lightProjAspectRatio = shadowMapFaceSizef.x() / shadowMapFaceSizef.y();

                    static constexpr float kOneOverSqrt2 = std::bit_cast<float>(0x3f3504f3u);
                    static Quaternion faceRotations[6] = {
                        {0.0f, kOneOverSqrt2, 0.0f, kOneOverSqrt2},  // -X
                        {0.0f, -kOneOverSqrt2, 0.0f, kOneOverSqrt2}, // +X
                        {-kOneOverSqrt2, 0.0f, 0.0f, kOneOverSqrt2}, // -Y
                        {kOneOverSqrt2, 0.0f, 0.0f, kOneOverSqrt2},  // +Y
                        {0.0f, 0.0f, 0.0f, 1.0f},                    // -Z
                        {0.0f, 1.0f, 0.0f, 0.0f}                     // +Z
                    };

                    for (int face = 0; face < 6; face++) {
                        auto shadowmapTile = sharedRendResources.allocateShadowmapTile(shadowmapFaceRectSize);
                        auto lightView = Transform3D(position, faceRotations[face], Vector3f::one())
                            .computeModelMatrix()
                            .inverseRigid();

                        auto lightProj = CameraComponent(0.001f, light.range, 93.0f, true)
                            .computeProjectionMatrix(lightProjAspectRatio);

                        auto lightProjview = lightProj * lightView;

                        auto shadowmapOffsetf = Vector2f(shadowmapTile.texelOffset.x(), shadowmapTile.texelOffset.y());
                        auto shadowmapAtlasSizef = Vector2f(kShadowMapAtlasSize.x(), kShadowMapAtlasSize.y());

                        auto shadowmapDescriptor = ShadowmapDescriptor {
                            .lightProjview = lightProjview,
                            .atlasOffset = shadowmapOffsetf.componentWiseDivide(shadowmapAtlasSizef),
                            .atlasRectSize = shadowMapFaceSizef.componentWiseDivide(shadowmapAtlasSizef)
                        };

                        sharedRendResources.shadowmapAtlasRenderingJobsPerAtlas()[shadowmapTile.shadowmapAtlasIndex].emplace(ShadowRenderingJob {
                            .shadowmapDescriptorIndex = static_cast<u32>(sharedRendResources.m_currShadowmapDescriptorCount),
                            .renderViewMatrix = lightView,
                            .renderFov = 90.0f,
                            .renderNearPlane = 0.001f,
                            .randerFarPlane = light.range,
                            .atlasViewportOffset = shadowmapTile.texelOffset,
                            .atlasViewportSize = light.shadowMapFaceSize
                        });

                        if (face == 0) shadowmapDescriptorIndexFirst = sharedRendResources.m_currShadowmapDescriptorCount;
                        shadowmapAtlasImageSrtIndices[face] = sharedRendResources.shadowmapAtlases()[shadowmapTile.shadowmapAtlasIndex].atlasSrtIndex.imageIndex;

                        memcpy(m_shadowMappingDescriptorBuffer.memoryHostPtr() + sharedRendResources.m_currShadowmapDescriptorCount * sizeof(ShadowmapDescriptor), &shadowmapDescriptor, sizeof(ShadowmapDescriptor));
                        sharedRendResources.m_currShadowmapDescriptorCount++;
                    }

                }


                auto pointlightData = PointlightData {
                    .position = position,
                    .lightRadiance = light.lightRadiance,
                    .inverseRangeSq = invRangeSq,
                    .shadowmapDescriptorIndexFirst = shadowmapDescriptorIndexFirst
                };
                for (int i = 0; i < 6; i++) pointlightData.shadowmapAtlasImageSrtIndices[i] = shadowmapAtlasImageSrtIndices[i];

                memcpy(m_pointlightsBuffer.memoryHostPtr() + m_currPointlightCount * sizeof(PointlightData), &pointlightData, sizeof(PointlightData));
                m_currPointlightCount++;

                break;
            }
            case LightType::Spot: {
                always_assert(m_currSpotlightCount < m_spotlightsBuffer.size() / sizeof(SpotlightData), "not enough space in spotlights buffer");

                auto cosInnerAngle = std::cos(degreesToRadians(light.spotlightInnerAngle));
                auto cosOuterAngle = std::cos(degreesToRadians(light.spotlightOuterAngle));
                auto invAngleRange = 1.0f / std::max(cosInnerAngle - cosOuterAngle, 1e-4f);

                u32 shadowmapAtlasSrtImageIndex = ~0u;

                if (light.castShadows) {
                    always_assert(sharedRendResources.m_currShadowmapDescriptorCount + 1 <= m_shadowMappingDescriptorBuffer.size() / sizeof(ShadowmapDescriptor), "not enough space in shadowmap descriptor buffer");

                    auto shadowmapRectSize = light.shadowMapFaceSize;
                    auto shadowmapTile = sharedRendResources.allocateShadowmapTile(shadowmapRectSize);

                    auto shadowMapFaceSizef = Vector2f(light.shadowMapFaceSize.x(), light.shadowMapFaceSize.y());
                    auto lightView = matrix.inverseRigid();
                    auto lightProjAspectRatio = shadowMapFaceSizef.x() / shadowMapFaceSizef.y();
                    auto lightProj = CameraComponent(0.001f, light.range, light.spotlightOuterAngle * 2.0f, true)
                        .computeProjectionMatrix(lightProjAspectRatio);
                    auto lightProjview = lightProj * lightView;

                    auto shadowmapOffsetf = Vector2f(shadowmapTile.texelOffset.x(), shadowmapTile.texelOffset.y());
                    auto shadowmapAtlasSizef = Vector2f(kShadowMapAtlasSize.x(), kShadowMapAtlasSize.y());

                    auto shadowmapDescriptor = ShadowmapDescriptor {
                        .lightProjview = lightProjview,
                        .atlasOffset = shadowmapOffsetf.componentWiseDivide(shadowmapAtlasSizef),
                        .atlasRectSize = shadowMapFaceSizef.componentWiseDivide(shadowmapAtlasSizef)
                    };

                    sharedRendResources.shadowmapAtlasRenderingJobsPerAtlas()[shadowmapTile.shadowmapAtlasIndex].emplace(ShadowRenderingJob {
                        .shadowmapDescriptorIndex = static_cast<u32>(sharedRendResources.m_currShadowmapDescriptorCount),
                        .renderViewMatrix = lightView,
                        .renderFov = light.spotlightOuterAngle * 2.0f,
                        .renderNearPlane = 0.001f,
                        .randerFarPlane = light.range,
                        .atlasViewportOffset = shadowmapTile.texelOffset,
                        .atlasViewportSize = light.shadowMapFaceSize
                    });

                    memcpy(m_shadowMappingDescriptorBuffer.memoryHostPtr() + sharedRendResources.m_currShadowmapDescriptorCount * sizeof(ShadowmapDescriptor), &shadowmapDescriptor, sizeof(ShadowmapDescriptor));
                    sharedRendResources.m_currShadowmapDescriptorCount++;
                    shadowmapAtlasSrtImageIndex = sharedRendResources.shadowmapAtlases()[shadowmapTile.shadowmapAtlasIndex].atlasSrtIndex.imageIndex;
                }

                auto spotlightData = SpotlightData {
                    .position = position,
                    .lightRadiance = light.lightRadiance,
                    .direction = rotationDir,
                    .cosOuterAngle = cosOuterAngle,
                    .inverseRangeSq = invRangeSq,
                    .inverseAngleRange = invAngleRange,
                    .shadowmapAtlasImageSrtIndex = shadowmapAtlasSrtImageIndex,
                    .shadowmapDescriptorIndex = static_cast<u32>(sharedRendResources.m_currShadowmapDescriptorCount) - 1u,
                };

                memcpy(m_spotlightsBuffer.memoryHostPtr() + m_currSpotlightCount * sizeof(SpotlightData), &spotlightData, sizeof(SpotlightData));
                m_currSpotlightCount++;

                break;
            }
        }
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
    auto jitteredProjviewInverse = cameraModelMatrix * jitteredProj.inverse().unwrapOr(Matrix4x4f::identity());

    auto cameraPos = cameraModelMatrix.decomposePosition();

    auto globdata = ShGlobalData {
        .jitteredProjview = jitteredProjview,
        .jitteredProjviewInverse = jitteredProjviewInverse,
        .projview = projview,
        .prevProjview = sharedRendResources.m_lastProjview,
        .prevProjviewNoTranslation = sharedRendResources.m_lastProjviewNoTranslation,
        .projviewInverse = cameraModelMatrix * projInverse,
        .projviewNoTranslationInverse = projviewNoTranslationInverse,
        .cameraModel = cameraModelMatrix,
        .cameraPos = cameraPos,
        .cameraNear = camera.nearPlane,
        .frameIndex = static_cast<u32>(frameIndex)
    };
    sharedRendResources.m_lastProjview = jitteredProjview;
    sharedRendResources.m_lastProjviewNoTranslation = projviewNoTranslation;

    memcpy(m_globalDataBuffer.memoryHostPtr(), &globdata, sizeof(ShGlobalData));

    computeLodLevels(renderingData, cameraPos, cameraPerspFocalLength);
}

auto FrameContextData::computeLodLevels(MRThreadsSharedDataLeaf& renderingData, Vector3f cameraPos, float cameraPerspFocalLength) -> void {
    if (m_entityMeshLodLevels.len() < renderingData.m_renderables.m_sparseToStorage.len()) {
        m_entityMeshLodLevels.resize(renderingData.m_renderables.m_sparseToStorage.len(), LodLevelInfo());
    }

    for (auto [i, renderable] : renderingData.m_renderables.m_storage.iter().enumerate()) {
        auto entity = renderingData.m_renderables.m_storageToEntity[i];

        auto& lodList = MeshAssetStorage::get().getLodList(renderable.meshAsset);
        auto bestAvailableLod = lodList.bestLodIndex.load(std::memory_order_acquire);
        if (bestAvailableLod == ~0u) {
            m_entityMeshLodLevels[entity.index()] = LodLevelInfo { .level = ~0u };
            continue;
        }

        auto objectPos = Vector3f::zero();
        auto objectUniformScale = 1.0_f32;
        if (renderingData.m_transforms.containsEntity(entity)) {
            auto& transformMatrix = renderingData.m_transforms.get(entity).m_transform;
            objectPos = transformMatrix.decomposePosition();

            float sx = Vector3f(transformMatrix[0, 0], transformMatrix[1, 0], transformMatrix[2, 0]).length();
            float sy = Vector3f(transformMatrix[0, 1], transformMatrix[1, 1], transformMatrix[2, 1]).length();
            float sz = Vector3f(transformMatrix[0, 2], transformMatrix[1, 2], transformMatrix[2, 2]).length();
            objectUniformScale = std::max({sx, sy, sz});
        }

        float screenPixels = lodList.computeScreenSpaceError(objectPos, cameraPos, cameraPerspFocalLength, objectUniformScale);

        u32 lodLevel = std::clamp(m_entityMeshLodLevels[entity.index()].level, bestAvailableLod, lodList.maxLodIndex);

        // See if the next LOD level's threshold is satisfied, and if so, then upgrade the level:
        while (lodLevel > bestAvailableLod) {
            auto upgradeScreenPixelsThreshold = lodList.lods[lodLevel - 1].screenSizeThreshold * lodList.lodHysteresisFactor;
            if (screenPixels < upgradeScreenPixelsThreshold) break;
            lodLevel--;
        }

        // Do likewise for downgrading the level:
        while (lodLevel < lodList.maxLodIndex) {
            auto downgradeScreenPixelsThreshold = lodList.lods[lodLevel].screenSizeThreshold / lodList.lodHysteresisFactor;
            if (screenPixels >= downgradeScreenPixelsThreshold) break;
            lodLevel++;
        }

        m_entityMeshLodLevels[entity.index()].level = lodLevel;
    }
}
} // namespace projnekomata