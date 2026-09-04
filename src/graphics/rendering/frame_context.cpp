module;
#include <string.h>
module projnekomata;
import projnekomata.corelib;
import vulkan;
import vk_mem_alloc;
import :graphics.rendering.frame_rendering_resources;
import :core.ecs.world.camera;
import :core.ecs.world.transform;
import :graphics.fontsystem.font_manager;
import :graphics.vulkan.vk_buffer;
import :graphics.meshsystem.mesh_asset_storage;
import :core.ui.ui_drawcmds;
import :graphics.vulkan.context;
import :graphics.texturesystem.texture_manager;
import :graphics.vulkan.vk_commands_barriers;
import :core.ecs.entity;
import :core.overloaded;
import :graphics.rendering.frame_context;
import :graphics.materialsystem.mat_manager;

namespace projnekomata::gfx {

using namespace projnekomata::math;

FrameContext::FrameContext(std::nullptr_t) {  }
FrameContext::FrameContext() {
    m_frameRenderingResources = FrameRenderingResources(2048);

    m_timestampsQueryPool = vkrhi::VulkanQueryPool::create(vk::QueryType::eTimestamp, 6, {});

    bool supportsPipelineStatisticsQuery = vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_hasPipelineStatisticsQuery;

    if (supportsPipelineStatisticsQuery) {
        m_pipelineStatisticsQueryPool = vkrhi::VulkanQueryPool::create(vk::QueryType::ePipelineStatistics, 1,
            vk::QueryPipelineStatisticFlagBits::eVertexShaderInvocations
                | vk::QueryPipelineStatisticFlagBits::eTessellationControlShaderPatches
                | vk::QueryPipelineStatisticFlagBits::eTessellationEvaluationShaderInvocations
                | vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations
        );
    }

}
auto FrameContext::waitForLastFrame() -> void {
    m_frameRenderingResources.frameDoneFence().waitForSignal(std::numeric_limits<u64>::max());
}

inline vk::Offset3D toOffset3D(const vk::Extent3D& extent) {
    return vk::Offset3D{
        static_cast<i32>(extent.width),
        static_cast<i32>(extent.height),
        static_cast<i32>(extent.depth)
    };
}

inline bool isObjectVisible(
    Vector3f objectPos, float objectBoundingSphereRadius,
    Matrix4x4f& cameraInverse,
    float perspectiveFov, float perspectiveAspectRatio, float perspectiveNear, float perspectiveFar
) {
    auto camspaceObjectPosHm = cameraInverse * Vector4f(objectPos.x(), objectPos.y(), objectPos.z(), 1.0f);
    auto camspaceObjectPos = Vector3f(camspaceObjectPosHm.x(), camspaceObjectPosHm.y(), camspaceObjectPosHm.z()) * -1.0f;

    // Near/far planes test
    if (camspaceObjectPos.z() + objectBoundingSphereRadius < perspectiveNear) return false;
    if (camspaceObjectPos.z() - objectBoundingSphereRadius > perspectiveFar) return false;

    // Left/right/top/bottom planes test
    float hy = perspectiveFov * 0.5f;
    float hx = std::atanf(std::tanf(hy) * perspectiveAspectRatio);
    float sx = std::sin(hx), sy = std::sin(hy);
    float cx = std::cos(hx), cy = std::cos(hy);

    if ( camspaceObjectPos.x() * cx + camspaceObjectPos.z() * sx < -objectBoundingSphereRadius) return false; // left
    if (-camspaceObjectPos.x() * cx + camspaceObjectPos.z() * sx < -objectBoundingSphereRadius) return false; // right
    if ( camspaceObjectPos.y() * cy + camspaceObjectPos.z() * sy < -objectBoundingSphereRadius) return false; // top
    if (-camspaceObjectPos.y() * cy + camspaceObjectPos.z() * sy < -objectBoundingSphereRadius) return false; // bottom

    return true;
}

auto FrameContext::execute(TransientRenderingResources& transientRenderingResources, SharedRenderingResources& sharedRenderingResources, vkrhi::VulkanSwapchain& swapchain,
    MRThreadsSharedDataLeaf& renderingData, MRThreadsSharedData& threadSharedData, bool recordStatistics) -> FrameResult {
    auto imageAcquire = swapchain.acquireNextImage(std::numeric_limits<u64>::max(), m_frameRenderingResources.imageAcquiredSemaphore());
    bool supportsPipelineStatisticsQuery = vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_hasPipelineStatisticsQuery;

    bool shouldRecreateSwapchainAfter = imageAcquire.second;
    if (imageAcquire.first.isNone()) {
        return { .shouldRecreateSwapchain = true, .stepPerFrameResources = false };
    }

    m_frameRenderingResources.frameDoneFence().reset();
    sharedRenderingResources.refitHysteresisStates(renderingData.m_renderables.m_sparseToStorage.size());
    m_numDrawcalls = 0;
    m_queryPoolsHaveResultsOnFinish = recordStatistics;

    // ---------------------------------------------------------------- Render Pass starts here ----------------------------------------------------------------

    CameraComponent firstCamera;
    WorldTransformComponent firstCameraTransform;
    bool firstCameraFound = false;

    for (auto [i, camera] : renderingData.m_cameras.m_storage.iter().enumerate()) {
        firstCamera = camera;

        auto cameraEntitySparseIndex = renderingData.m_cameras.m_storageToEntity[i];;

        if (renderingData.m_transforms.containsEntity(cameraEntitySparseIndex)) {
            firstCameraTransform = renderingData.m_transforms.get(cameraEntitySparseIndex);
        } else {
            firstCameraTransform = WorldTransformComponent{};
            log::warn("Camera #{} has no transform!", i);
        }
        firstCameraFound = true;
        break;
    }

    if (!firstCameraFound) {
        log::warn("No camera found! Will use a default one");
        firstCamera = CameraComponent{};
        firstCamera.nearPlane = 0.1f;
        firstCamera.farPlane = 1000.0f;
        firstCamera.fov = 75.0f;
        firstCamera.renderingEnable = true;
        firstCameraTransform = WorldTransformComponent{};
    }
    auto cameraPos = firstCameraTransform.m_transform.decomposePosition();

    auto cameraViewMatrix = firstCameraTransform.m_transform.inverseRigid();

    math::Vector2f renderingArea = Vector2f(static_cast<f32>(transientRenderingResources.postSmaaImage().extent().width), static_cast<f32>(transientRenderingResources.postSmaaImage().extent().height));

    float aspectRatio = static_cast<float>(transientRenderingResources.postSmaaImage().extent().width) / static_cast<float>(transientRenderingResources.postSmaaImage().extent().height);
    float perspFocalLength = renderingArea.y() / (2.0f * std::tan(0.5f * degreesToRadians(firstCamera.fov)));

    m_frameRenderingResources.prepareBuffers(renderingData, sharedRenderingResources, firstCamera, firstCameraTransform, aspectRatio, renderingData.m_frameIndex);
    auto& swapchainImage = swapchain.imageAtIndex(imageAcquire.first.unwrap());

    m_frameRenderingResources.commandPool().reset();

    auto& cb = m_frameRenderingResources.commandBuffer().vkCommandBuffer();

    auto beginInfo = vk::CommandBufferBeginInfo{}
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vkrhi::vkCheckResult(cb.begin(beginInfo));

    if (recordStatistics) {
        cb.resetQueryPool(m_timestampsQueryPool.vkQueryPool(), 0, m_timestampsQueryPool.queryCount());
        if (supportsPipelineStatisticsQuery) cb.resetQueryPool(m_pipelineStatisticsQueryPool.vkQueryPool(), 0, m_pipelineStatisticsQueryPool.queryCount());
    }

    // ---- Font Rasterization ---------------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanBuffer stagingBuffer = nullptr;

    auto& fontAtlas = threadSharedData.m_fontAtlas;
    auto& pixelBuffer = renderingData.m_fontsUploadPixelBuffer;
    auto& bufferImageCopyRegions = renderingData.m_fontsCopyRegions;
    auto& newImageIndices = renderingData.m_fontsNewImageIndices;

    if (bufferImageCopyRegions.len() > 0) {
        if (pixelBuffer.len() == 0) panic("font data copy was requested, but the pixel buffer len is 0");

        stagingBuffer = vkrhi::VulkanBuffer::builder()
            .len(pixelBuffer.len())
            .usage(vk::BufferUsageFlagBits::eTransferSrc)
            .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
            .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
            .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
            .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
            .build();

        memcpy(stagingBuffer.memoryHostPtr(), pixelBuffer.data(), pixelBuffer.len());

        // Prepare for copy
        auto barriers = vkrhi::VulkanPipelineBarriers::builder();
        for (const auto& atlasImageIndex : bufferImageCopyRegions.keys()) {
            // For images that are newly created, transition from eUndefined instead
            if (newImageIndices.contains(atlasImageIndex)) {
                barriers.insertImageMemoryBarrier(fontAtlas.m_atlasTextures[atlasImageIndex].image,
                    vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                    vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite
                );
                continue;
            }

            // Transition from eShaderReadOnlyOptimal for all others
            barriers.insertImageMemoryBarrier(fontAtlas.m_atlasTextures[atlasImageIndex].image,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite
            );
        }
        barriers.flush(m_frameRenderingResources.commandBuffer());

        // Run copies
        for (const auto& [atlasImageIndex, regions] : bufferImageCopyRegions.iter()) {
            auto copyInfo = vk::CopyBufferToImageInfo2{}
                .setDstImage(fontAtlas.m_atlasTextures[atlasImageIndex].image.vkImage())
                .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSrcBuffer(stagingBuffer.vkBuffer())
                .setRegions(regions);

            cb.copyBufferToImage2(copyInfo);
        }

        // Prepare for usage
        auto barriers2 = vkrhi::VulkanPipelineBarriers::builder();
        for (const auto& atlasImageIndex : bufferImageCopyRegions.keys()) {
            barriers2.insertImageMemoryBarrier(fontAtlas.m_atlasTextures[atlasImageIndex].image,
                vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead
            );
        }
        barriers2.flush(m_frameRenderingResources.commandBuffer());
    }

    auto vkRenderingArea = vk::Extent2D{transientRenderingResources.postSmaaImage().extent().width, transientRenderingResources.postSmaaImage().extent().height};
    auto viewport = vk::Viewport{}
        .setWidth(static_cast<f32>(vkRenderingArea.width))
        .setHeight(static_cast<f32>(vkRenderingArea.height))
        .setMinDepth(0.0)
        .setMaxDepth(1.0);
    auto scissor = vk::Rect2D{}.setExtent(vkRenderingArea);
    auto globaldataAddr = m_frameRenderingResources.globalDataBuffer().memoryDevicePtr();

    // ---- Velocity Buffer Clear ------------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.velocityBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto velocityBufferClAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.velocityBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto velocityBufferClRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(velocityBufferClAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(velocityBufferClRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_velbufferBgPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_velbufferBgLayout, vk::PipelineBindPoint::eGraphics);
    cb.pushConstants<vk::DeviceAddress>(sharedRenderingResources.m_velbufferBgLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, globaldataAddr);
    cb.draw(3, 1, 0, 0);
    cb.endRendering();

    // ---- Deferred Geometry Stage ----------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.overdrawCountersImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    cb.clearColorImage(transientRenderingResources.overdrawCountersImage().vkImage(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{}.setUint32({0, 0, 0, 0}), transientRenderingResources.overdrawCountersImage().subresourceRangeFull());

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.overdrawCountersImage(),
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        )
        .insertImageMemoryBarrier(transientRenderingResources.albedoAndRoughnessBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(transientRenderingResources.emissiveBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(transientRenderingResources.normalBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(transientRenderingResources.metallicAndAoBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(transientRenderingResources.velocityBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(transientRenderingResources.depthBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, m_timestampsQueryPool.vkQueryPool(), 0);
        if (supportsPipelineStatisticsQuery) cb.beginQuery(m_pipelineStatisticsQueryPool.vkQueryPool(), 0, {});
    }

    auto albedoAndRoughnessAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.albedoAndRoughnessBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto emissiveAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.emissiveBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto normalAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.normalBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto metallicAndAoAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.metallicAndAoBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto velocityBufferAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.velocityBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto depthAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.depthBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearDepthStencilValue{}.setDepth(0.0f));

    auto colorAttachments = std::array<vk::RenderingAttachmentInfo, 5>{albedoAndRoughnessAttachmentInfo, emissiveAttachmentInfo, normalAttachmentInfo, metallicAndAoAttachmentInfo, velocityBufferAttachmentInfo};
    auto deferredGeomRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(colorAttachments)
        .setPDepthAttachment(&depthAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(deferredGeomRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    auto& globPipelineLayout = MaterialManager::get().globalPipelineLayout();

    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), globPipelineLayout, vk::PipelineBindPoint::eGraphics);
    auto uboDeviceAddr = m_frameRenderingResources.transformsBuffer().memoryDevicePtr();

    struct RenderPushConstantData {
        vk::DeviceAddress objectUniformAddr;
        vk::DeviceAddress vertexbufferAddr;
        vk::DeviceAddress globaldataAddr;
        vk::DeviceAddress materialDataAddr;
        vk::DeviceAddress textureToImageShaderIndexTableAddr;
        vk::DeviceAddress textureToSamplerShaderIndexTableAddr;
    };

    for (auto [i, renderable] : renderingData.m_renderables.m_storage.iter().enumerate()) {
        // Get the LOD list for the renderable and skip it if no LODs are available
        auto& lodList = MeshAssetStorage::get().getLodList(renderable.meshAsset);
        auto bestAvailableLod = lodList.bestLodIndex.load(std::memory_order_acquire);
        if (bestAvailableLod == ~0u) continue;

        // Copy its transforms addr to push constants
        vk::DeviceAddress uboFinalAddr = uboDeviceAddr + i * sizeof(Transforms);

        // Pick an LOD
        Vector3f objectPos = Vector3f(0.0f);
        float objectUniformScale = 1.0f;
        ecs::Entity ent = renderingData.m_renderables.m_storageToEntity[i];
        if (renderingData.m_transforms.containsEntity(ent)) {
            auto& transformMatrix = renderingData.m_transforms.get(ent).m_transform;
            float sx = Vector3f(transformMatrix[0, 0], transformMatrix[1, 0], transformMatrix[2, 0]).length();
            float sy = Vector3f(transformMatrix[0, 1], transformMatrix[1, 1], transformMatrix[2, 1]).length();
            float sz = Vector3f(transformMatrix[0, 2], transformMatrix[1, 2], transformMatrix[2, 2]).length();
            objectUniformScale = std::max({sx, sy, sz});
            objectPos = transformMatrix.decomposePosition();
        }

        // See if the object is visible
        if (!isObjectVisible(objectPos, objectUniformScale * lodList.boundingSphereRadius, cameraViewMatrix, degreesToRadians(firstCamera.fov), aspectRatio, firstCamera.nearPlane, firstCamera.farPlane)) {
            continue;
        }

        float screenPixels = lodList.computeScreenSpaceError(objectPos, cameraPos, perspFocalLength, objectUniformScale);

        MeshHysteresisState& hysteresisState = sharedRenderingResources.getHysteresisState(ent.index());
        // Hysteresis states can be reused between entity creations/destructions. This prevents possibly using an unavailable LOD.
        hysteresisState.currentLod = std::clamp(hysteresisState.currentLod, bestAvailableLod, lodList.maxLodIndex);

        while (hysteresisState.currentLod > bestAvailableLod) {
            // must test next level LOD to see if satisfies new threshold
            float upgradeScreenPixelsThreshold = lodList.lods[hysteresisState.currentLod - 1].screenSizeThreshold * lodList.lodHysteresisFactor;
            if (screenPixels >= upgradeScreenPixelsThreshold) {
                hysteresisState.currentLod--;
                // log::info("Object {} upgraded to LOD level {}", ent.index(), hysteresisState.currentLod);
            } else {
                break;
            }
        }

        while (hysteresisState.currentLod < lodList.maxLodIndex) {
            // tests current level LOD to see if no longer satisfies current threshold
            float downgradeScreenPixelsThreshold = lodList.lods[hysteresisState.currentLod].screenSizeThreshold / lodList.lodHysteresisFactor;
            if (screenPixels < downgradeScreenPixelsThreshold) {
                hysteresisState.currentLod++;
                // log::info("Object {} downgraded to LOD level {}", ent.index(), hysteresisState.currentLod);
            } else {
                break;
            }
        }

        auto& lod = lodList.lods[hysteresisState.currentLod];

        auto vboDeviceAddr = lod.meshSuballocation.vertexBuffer.deviceAddress;
        auto materialDataStride = renderable.material.shader->materialPropStructSize();
        auto materialDataAddr = m_frameRenderingResources.materialPropBuffer(materialDataStride).memoryDevicePtr() + materialDataStride * renderable.material.properties.propertiesIndex;

        auto pushconstData = RenderPushConstantData {
            .objectUniformAddr = uboFinalAddr,
            .vertexbufferAddr = vboDeviceAddr,
            .globaldataAddr = globaldataAddr,
            .materialDataAddr = materialDataAddr,
            .textureToImageShaderIndexTableAddr = m_frameRenderingResources.textureToSrtImageIDBuffer().memoryDevicePtr(),
            .textureToSamplerShaderIndexTableAddr = m_frameRenderingResources.textureToSrtSamplerIDBuffer().memoryDevicePtr()
        };

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, renderable.material.shader->pipeline().vkPipeline());
        cb.bindIndexBuffer(lod.meshSuballocation.indexBuffer.buffer, lod.meshSuballocation.indexBuffer.offset, vk::IndexType::eUint32);
        cb.pushConstants<RenderPushConstantData>(globPipelineLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, pushconstData);
        cb.drawIndexed(lod.meshSuballocation.indexBuffer.size / sizeof(u32), 1, 0, 0, 0);
        m_numDrawcalls++;
    }

    cb.endRendering();

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe, m_timestampsQueryPool.vkQueryPool(), 1);
        if (supportsPipelineStatisticsQuery) cb.endQuery(m_pipelineStatisticsQueryPool.vkQueryPool(), 0);
    }

    // ---- Deferred Lighting Stage ----------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.albedoAndRoughnessBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.emissiveBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.normalBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.metallicAndAoBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.velocityBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.depthBuffer(),
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.colorBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, m_timestampsQueryPool.vkQueryPool(), 2);
    }

    u32 skyboxTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingResources.m_skyCubemap.index];
    u32 irradianceTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingResources.m_skyIrradianceCubemap.index];
    u32 prefilterTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingResources.m_skyPrefilterCubemap.index];
    u32 iblLutTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingResources.m_brdfLUT.index];
    u32 linearSamplerId = renderingData.m_textureToSamplerShaderIndexSnapshot[sharedRenderingResources.m_skyCubemap.index];
    u32 nearestSamplerId = TextureManager::get().samplerCache().acquireSampler(
        SamplerParams::defaultValues().setMinFilter(vk::Filter::eNearest).setMagFilter(vk::Filter::eNearest).setMipmapMode(vk::SamplerMipmapMode::eNearest).setMaxLod(0.0f)
    );

    auto drawImageAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.colorBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto lightingstageRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(drawImageAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(lightingstageRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_mainLightingPassPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_mainLightingPassLayout, vk::PipelineBindPoint::eGraphics);

    struct LightingStagePushConstantData {
        vk::DeviceAddress globaldataAddr;
        vk::DeviceAddress pointlightsAddr;
        u32 pointlightCount;
        u32 depthTextureIndex;
        u32 albedoAndRoughnessTextureIndex;
        u32 emissiveTextureIndex;
        u32 normalTextureIndex;
        u32 metallicAoTextureIndex;
        u32 skyboxTextureId;
        u32 irradianceTextureId;
        u32 prefiltTextureId;
        u32 iblLutTextureId;
        u32 linearSamplerIndex;
        u32 nearestSamplerIndex;
    };

    auto pushconstData = LightingStagePushConstantData {
        .globaldataAddr = globaldataAddr,
        .pointlightsAddr = m_frameRenderingResources.pointlightsBuffer().memoryDevicePtr(),
        .pointlightCount = static_cast<u32>(renderingData.m_pointlights.m_storage.len()),
        .depthTextureIndex = transientRenderingResources.depthBufferIndex().imageIndex,
        .albedoAndRoughnessTextureIndex = transientRenderingResources.albedoAndRoughnessBufferIndex().imageIndex,
        .emissiveTextureIndex = transientRenderingResources.emissiveBufferIndex().imageIndex,
        .normalTextureIndex = transientRenderingResources.normalBufferIndex().imageIndex,
        .metallicAoTextureIndex = transientRenderingResources.metallicAndAoBufferIndex().imageIndex,
        .skyboxTextureId = skyboxTextureId,
        .irradianceTextureId = irradianceTextureId,
        .prefiltTextureId = prefilterTextureId,
        .iblLutTextureId = iblLutTextureId,
        .linearSamplerIndex = linearSamplerId,
        .nearestSamplerIndex = nearestSamplerId
    };

    cb.pushConstants<LightingStagePushConstantData>(sharedRenderingResources.m_mainLightingPassLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushconstData);
    cb.draw(3, 1, 0, 0);

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe, m_timestampsQueryPool.vkQueryPool(), 3);
    }

    cb.endRendering();

    // ---- SMAA -----------------------------------------------------------------------------------------------------------------------------------------------


    auto smaaRtMetrics = Vector4f(1.0f / renderingArea.x(), 1.0f / renderingArea.y(), renderingArea.x(), renderingArea.y());

    auto smaaLinearSamplerSrtID = TextureManager::get().samplerCache().acquireSampler(
        SamplerParams::defaultValues()
            .setMinFilter(vk::Filter::eLinear)
            .setMagFilter(vk::Filter::eLinear)
            .setMipmapMode(vk::SamplerMipmapMode::eNearest)
            .setMaxLod(0.0f)
    );

    auto smaaNearestSamplerSrtID = TextureManager::get().samplerCache().acquireSampler(
        SamplerParams::defaultValues()
            .setMinFilter(vk::Filter::eNearest)
            .setMagFilter(vk::Filter::eNearest)
            .setMipmapMode(vk::SamplerMipmapMode::eNearest)
            .setMaxLod(0.0f)
    );

    u32 smaaAreaTextureSrtID = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingResources.m_smaaAreaTexture.index];
    u32 smaaSearchTextureSrtID = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingResources.m_smaaSearchTexture.index];

    // -------- [Stage 1] Edge detection

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.colorBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.smaaEdgesImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, m_timestampsQueryPool.vkQueryPool(), 4);
    }

    auto edgesImageAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.smaaEdgesImage().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 0.0f});

    auto smaaEdgeDetectionRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(edgesImageAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(smaaEdgeDetectionRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_smaaEdgeDetectPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_smaaEdgeDetectLayout, vk::PipelineBindPoint::eGraphics);

    struct SmaaEdgeDetectionPushConstantData {
        Vector4f rtMetrics;
        u32 colorBufferSrtID;
        u32 linearSamplerSrtID;
        u32 nearestSamplerSrtID;
    };

    auto smaaEdgeDetectionPushconstData = SmaaEdgeDetectionPushConstantData {
        .rtMetrics = smaaRtMetrics,
        .colorBufferSrtID = transientRenderingResources.colorBufferUnormViewIndex().imageIndex,
        .linearSamplerSrtID = smaaLinearSamplerSrtID,
        .nearestSamplerSrtID = smaaNearestSamplerSrtID
    };

    cb.pushConstants<SmaaEdgeDetectionPushConstantData>(sharedRenderingResources.m_smaaEdgeDetectLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaEdgeDetectionPushconstData);
    cb.draw(3, 1, 0, 0);

    cb.endRendering();

    // -------- [Stage 2] Blend weights

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.smaaEdgesImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.smaaWeightsImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto weightsImageAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.smaaWeightsImage().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 0.0f});

    auto smaaBlendWeightsRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(weightsImageAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(smaaBlendWeightsRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_smaaBlendWeightPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_smaaBlendWeightLayout, vk::PipelineBindPoint::eGraphics);

    struct SmaaBlendWeightPushConstantData {
        Vector4f rtMetrics;
        u32 edgesImageSrtID;
        u32 areaTextureSrtID;
        u32 searchTextureSrtID;
        u32 linearSamplerSrtID;
        u32 nearestSamplerSrtID;
        u32 frameIndex;
    };

    auto smaaBlendWeightPushconstData = SmaaBlendWeightPushConstantData {
        .rtMetrics = smaaRtMetrics,
        .edgesImageSrtID = transientRenderingResources.smaaEdgesImageIndex().imageIndex,
        .areaTextureSrtID = smaaAreaTextureSrtID,
        .searchTextureSrtID = smaaSearchTextureSrtID,
        .linearSamplerSrtID = smaaLinearSamplerSrtID,
        .nearestSamplerSrtID = smaaNearestSamplerSrtID,
        .frameIndex = static_cast<u32>(renderingData.m_frameIndex)
    };

    cb.pushConstants<SmaaBlendWeightPushConstantData>(sharedRenderingResources.m_smaaBlendWeightLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaBlendWeightPushconstData);
    cb.draw(3, 1, 0, 0);

    cb.endRendering();

    // -------- [Stage 3] Neighborhood blend

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.smaaWeightsImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.postSmaaImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eBlit, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto finalDrawBufferUnormAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.postSmaaImage().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 0.0f});

    auto smaaNeighborhoodBlendRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(finalDrawBufferUnormAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(smaaNeighborhoodBlendRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_smaaNeighborhoodBlendPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_smaaNeighborhoodBlendLayout, vk::PipelineBindPoint::eGraphics);

    struct SmaaNeighborhoodBlendPushConstantData {
        Vector4f rtMetrics;
        u32 colorBufferSrtID;
        u32 weightsImageSrtID;
        u32 velocityBufferSrtID;
        u32 linearSamplerSrtID;
        u32 nearestSamplerSrtID;
    };

    auto smaaNeighborhoodBlendPushconstData = SmaaNeighborhoodBlendPushConstantData {
        .rtMetrics = smaaRtMetrics,
        .colorBufferSrtID = transientRenderingResources.colorBufferIndex().imageIndex,
        .weightsImageSrtID = transientRenderingResources.smaaWeightsImageIndex().imageIndex,
        .velocityBufferSrtID = transientRenderingResources.velocityBufferIndex().imageIndex,
        .linearSamplerSrtID = smaaLinearSamplerSrtID,
        .nearestSamplerSrtID = smaaNearestSamplerSrtID
    };

    cb.pushConstants<SmaaNeighborhoodBlendPushConstantData>(sharedRenderingResources.m_smaaNeighborhoodBlendLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaNeighborhoodBlendPushconstData);
    cb.draw(3, 1, 0, 0);

    cb.endRendering();

    // -------- [Stage 4] Temporal resolve

    auto frameParity = (renderingData.m_frameIndex & 1) == 0;
    auto& currentColorBuffer = frameParity ? transientRenderingResources.smaaColorResolvedBuffer0() : transientRenderingResources.smaaColorResolvedBuffer1();
    auto& currentColorBufferUnorm = frameParity ? transientRenderingResources.smaaColorResolvedBuffer0UnormView() : transientRenderingResources.smaaColorResolvedBuffer1UnormView();

    auto smaaNeighborhoodResolvedBufferIndex = transientRenderingResources.postSmaaImageUnormViewIndex();
    auto lastColorBufferIndex = frameParity ? transientRenderingResources.smaaColorResolvedBuffer1UnormViewIndex() : transientRenderingResources.smaaColorResolvedBuffer0UnormViewIndex();

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.postSmaaImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(transientRenderingResources.finalImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eBlit, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(currentColorBuffer,
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto currentColorUnormAttachment = vk::RenderingAttachmentInfo{}
        .setImageView(currentColorBufferUnorm.vkImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto finalImageUnormAttachment = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.finalImageUnormView().vkImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto attachments = std::array<vk::RenderingAttachmentInfo, 2>{currentColorUnormAttachment, finalImageUnormAttachment};

    auto smaaTemporalResolveRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(attachments)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(smaaTemporalResolveRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_smaaTemporalResolvePipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_smaaTemporalResolveLayout, vk::PipelineBindPoint::eGraphics);

    struct SmaaTemporalResolvePushConstants {
        Vector4f rtMetrics;
        u32 colorTextureSrtID;
        u32 lastResolvedColorTextureSrtID;
        u32 velocityBufferSrtID;
        u32 smaaLinearSamplerSrtID;
        u32 smaaNearestSamplerSrtID;
    };

    auto smaaTemporalResolvePushConstants = SmaaTemporalResolvePushConstants {
        .rtMetrics = smaaRtMetrics,
        .colorTextureSrtID = transientRenderingResources.postSmaaImageUnormViewIndex().imageIndex,
        .lastResolvedColorTextureSrtID = lastColorBufferIndex.imageIndex,
        .velocityBufferSrtID = transientRenderingResources.velocityBufferIndex().imageIndex,
        .smaaLinearSamplerSrtID = smaaLinearSamplerSrtID,
        .smaaNearestSamplerSrtID = smaaNearestSamplerSrtID
    };

    cb.pushConstants<SmaaTemporalResolvePushConstants>(sharedRenderingResources.m_smaaTemporalResolveLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaTemporalResolvePushConstants);
    cb.draw(3, 1, 0, 0);
    cb.endRendering();

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe, m_timestampsQueryPool.vkQueryPool(), 5);
    }
    // ---- Temporary : Quad Overdraw Vis ----------------------------------------------------------------------------------------------------------------------
/*
    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.finalImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto finalDrawBufferAttachmentInfodbg = vk::RenderingAttachmentInfo{}
    .setImageView(transientRenderingResources.finalImage().vkImageViewWholeSize())
    .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
    .setLoadOp(vk::AttachmentLoadOp::eLoad)
    .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto finalDrawBufferDbgRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(finalDrawBufferAttachmentInfodbg)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(finalDrawBufferDbgRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_quadOverdrawVisPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_quadOverdrawVisLayout, vk::PipelineBindPoint::eGraphics);
    cb.pushConstants<u32>(sharedRenderingResources.m_quadOverdrawVisLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, transientRenderingResources.overdrawCountersImageIndex().imageIndex);
    cb.draw(3, 1, 0, 0);
    cb.endRendering();
*/
    // ---- UI -------------------------------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.finalImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto finalDrawBufferAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(transientRenderingResources.finalImage().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto uiRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(finalDrawBufferAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(uiRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    auto textInstanceBuffers = Vec<vkrhi::VulkanBuffer>::create();

    auto queuesForBuffer = vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_queueFamilies[vkrhi::QueueFamily::Graphics];
    for (const auto& uiDrawCmd : renderingData.m_uiDrawCmds) {
        match(uiDrawCmd,
            [&](const ui::UiRectDrawCmd& drawCmd) {
                struct PushConstants {
                    Vector2f ssBegin, ssEnd;
                    Vector2f screenSize;
                    Vector4f color;
                };

                PushConstants pushConstants = {
                    .ssBegin = drawCmd.ssBegin,
                    .ssEnd = drawCmd.ssEnd,
                    .screenSize = renderingArea,
                    .color = drawCmd.color.asRgba32Float()
                };

                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_uiRectRendererPipeline.vkPipeline());
                cb.pushConstants<PushConstants>(sharedRenderingResources.m_uiRectRendererLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
                cb.draw(4, 1, 0, 0);
            },
            [&](const ui::UiTextureDrawCmd& drawCmd) {
                struct PushConstants {
                    Vector2f ssBegin, ssEnd;
                    Vector2f screenSize;
                    Vector2f texcoordBegin, texcoordEnd;
                    u32 textureIndex;
                    u32 samplerIndex;
                };

                u32 textureIndex = renderingData.m_textureToImageShaderIndexSnapshot[drawCmd.texture.index];
                u32 samplerIndex = renderingData.m_textureToSamplerShaderIndexSnapshot[drawCmd.texture.index];

                PushConstants pushConstants = {
                    .ssBegin = drawCmd.ssBegin,
                    .ssEnd = drawCmd.ssEnd,
                    .screenSize = renderingArea,
                    .texcoordBegin = drawCmd.texcoordBegin,
                    .texcoordEnd = drawCmd.texcoordEnd,
                    .textureIndex = textureIndex,
                    .samplerIndex = samplerIndex
                };
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_uiTextureRendererPipeline.vkPipeline());
                TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_uiTextureRendererLayout, vk::PipelineBindPoint::eGraphics);
                cb.pushConstants<PushConstants>(sharedRenderingResources.m_uiTextureRendererLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
                cb.draw(4, 1, 0, 0);
            },
            [&](const ui::UiTextDrawCmd& drawCmd) {
                auto& shapedText = drawCmd.glyphs;
                if (shapedText.isEmpty()) return;

                auto buffer = vkrhi::VulkanBuffer::builder()
                    .len(shapedText.len() * sizeof(GlyphInstance))
                    .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer)
                    .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
                    .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
                    .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
                    .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
                    .build();

                memcpy(buffer.memoryHostPtr(), shapedText.data(), shapedText.size() * sizeof(GlyphInstance));

                u32 sampler2 = TextureManager::get().samplerCache().acquireSampler(
                    SamplerParams::defaultValues().setMinFilter(vk::Filter::eNearest).setMagFilter(vk::Filter::eNearest).setMipmapMode(vk::SamplerMipmapMode::eNearest).setMaxLod(0.0f)
                );
                textInstanceBuffers.emplace(std::move(buffer));

                struct PushConstants {
                    vk::DeviceAddress instanceBuffer;
                    Vector2f ssGlobalOffset;
                    Vector2f screenSize;
                    u32 samplerSrtID;
                    Vector4f color;
                };

                PushConstants pushConstants = {
                    .instanceBuffer = buffer.memoryDevicePtr(),
                    .ssGlobalOffset = drawCmd.ssPosition.round(),
                    .screenSize = renderingArea,
                    .samplerSrtID = sampler2,
                    .color = drawCmd.color.asRgba32Float()
                };

                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingResources.m_bitmapFontRendererPipeline.vkPipeline());
                TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingResources.m_bitmapFontRendererLayout, vk::PipelineBindPoint::eGraphics);

                cb.pushConstants<PushConstants>(sharedRenderingResources.m_bitmapFontRendererLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
                cb.draw(4, shapedText.size(), 0, 0);
            }
        );
    }

    cb.endRendering();

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(transientRenderingResources.finalImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead
        )
        .insertImageMemoryBarrier(swapchainImage,
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto blitSubres = vk::ImageSubresourceLayers{}
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setMipLevel(0);

    auto srcOffsets = std::array<vk::Offset3D, 2>{{{0, 0, 0}, {toOffset3D(transientRenderingResources.postSmaaImage().extent())}}};

    auto blit = vk::ImageBlit2{}
        .setSrcSubresource(blitSubres)
        .setDstSubresource(blitSubres)
        .setSrcOffsets(srcOffsets)
        .setDstOffsets(srcOffsets);

    auto blitInfo = vk::BlitImageInfo2{}
        .setSrcImage(transientRenderingResources.finalImage().vkImage())
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(swapchainImage.vkImage())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setFilter(vk::Filter::eLinear)
        .setRegions(blit);

    cb.blitImage2(blitInfo);

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(currentColorBuffer,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(swapchainImage,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone
        )
        .flush(m_frameRenderingResources.commandBuffer());

    vkrhi::vkCheckResult(cb.end());

    vkrhi::VulkanContext::get().vkQueueGraphics().submitOneCommandBufferWithBinarySemaphores(
        cb,
        {}, {},
        m_frameRenderingResources.imageAcquiredSemaphore(), swapchainImage.vkSemaphoreImagePresent(),
        vk::PipelineStageFlagBits2::eBlit, vk::PipelineStageFlagBits2::eBlit,
        Some(std::ref(m_frameRenderingResources.frameDoneFence()))
    );

    vkrhi::VulkanContext::get().antiLagPacePresent(renderingData.m_frameIndex, 0);
    auto presentResult = vkrhi::VulkanContext::get().vkQueuePresent().submitPresent(swapchain, swapchainImage.vkSemaphoreImagePresent(), imageAcquire.first.unwrap());

    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        return { .shouldRecreateSwapchain = true, .stepPerFrameResources = true };
    }

    return { .shouldRecreateSwapchain = shouldRecreateSwapchainAfter, .stepPerFrameResources = true };
}

} // namespace projnekomata