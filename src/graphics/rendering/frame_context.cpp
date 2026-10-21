module;
#include <tracy/Tracy.hpp>
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
    m_frameRenderingResources = FrameContextData(2048);

    m_timestampsQueryPool = vkrhi::VulkanQueryPool::create(vk::QueryType::eTimestamp, static_cast<u32>(FrameContextTimestampIndex::CountDiscrim), {});

    bool supportsPipelineStatisticsQuery = vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_hasPipelineStatisticsQuery;

    if (supportsPipelineStatisticsQuery) {
        m_pipelineStatisticsQueryPool = vkrhi::VulkanQueryPool::create(vk::QueryType::ePipelineStatistics, 2,
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
    const Matrix4x4f& cameraInverse,
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

static auto& cvRdBloomRadius = CvarManager::get().registerCvar<float>("rd.bloomradius", 0.005f);
static auto& cvRdBloomStrength = CvarManager::get().registerCvar<float>("rd.bloomstrength", 0.06f);

auto FrameContext::execute(RenderingResources& renderingResources, SharedRenderingData& sharedRenderingData, vkrhi::VulkanSwapchain& swapchain,
    MRThreadsSharedDataLeaf& renderingData, MRThreadsSharedData& threadSharedData, bool recordStatistics) -> FrameResult {

    std::pair<Option<u32>, bool> imageAcquire = { None, false };

    {
        ZoneScopedN("vkAcquireNextImageKHR")
        imageAcquire = swapchain.acquireNextImage(std::numeric_limits<u64>::max(), m_frameRenderingResources.imageAcquiredSemaphore());
    }

    bool supportsPipelineStatisticsQuery = vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_hasPipelineStatisticsQuery;

    bool shouldRecreateSwapchainAfter = imageAcquire.second;
    if (imageAcquire.first.isNone()) {
        return { .shouldRecreateSwapchain = true, .stepPerFrameResources = false };
    }

    m_frameRenderingResources.frameDoneFence().reset();
    sharedRenderingData.refitHysteresisStates(renderingData.m_renderables.m_sparseToStorage.size());
    m_numDrawcalls = 0;
    m_numDrawcallsShadows = 0;
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

    auto vkRenderingArea = vk::Extent2D{renderingResources.postSmaaImage().extent().width, renderingResources.postSmaaImage().extent().height};
    auto renderingArea = Vector2f(static_cast<f32>(vkRenderingArea.width), static_cast<f32>(vkRenderingArea.height));
    float aspectRatio = renderingArea.x() / renderingArea.y();
    float perspFocalLength = renderingArea.y() / (2.0f * std::tan(0.5f * degreesToRadians(firstCamera.fov)));

    m_frameRenderingResources.prepareBuffers(renderingData, sharedRenderingData, firstCamera, firstCameraTransform, aspectRatio, perspFocalLength, renderingData.m_frameIndex);
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

    auto viewport = vk::Viewport{}
        .setWidth(static_cast<f32>(vkRenderingArea.width))
        .setHeight(static_cast<f32>(vkRenderingArea.height))
        .setMinDepth(0.0)
        .setMaxDepth(1.0);
    auto scissor = vk::Rect2D{}.setExtent(vkRenderingArea);
    auto globaldataAddr = m_frameRenderingResources.globalDataBuffer().memoryDevicePtr();

    // ---- Velocity Buffer Clear ------------------------------------------------------------------------------------------------------------------------------

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::CbStart_MvClear));
    }

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.velocityBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto velocityBufferClAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.velocityBuffer().vkImageViewWholeSize())
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

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_velbufferBgPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_velbufferBgLayout, vk::PipelineBindPoint::eGraphics);
    cb.pushConstants<vk::DeviceAddress>(sharedRenderingData.m_velbufferBgLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, globaldataAddr);
    cb.draw(3, 1, 0, 0);
    cb.endRendering();

    // ---- Deferred Geometry Stage ----------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.overdrawCountersImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    cb.clearColorImage(renderingResources.overdrawCountersImage().vkImage(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{}.setUint32({0, 0, 0, 0}), renderingResources.overdrawCountersImage().subresourceRangeFull());

    auto bloomReductionMipCount = renderingResources.emissiveAndBloomBuffer().mipLevels() - 1;
    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.overdrawCountersImage(),
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        )
        .insertImageMemoryBarrier(renderingResources.albedoAndRoughnessBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrierSubresource(renderingResources.emissiveAndBloomBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        )
        .insertImageMemoryBarrier(renderingResources.normalBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(renderingResources.metallicAndAoBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(renderingResources.velocityBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .insertImageMemoryBarrier(renderingResources.depthBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    for (auto& atlas : sharedRenderingData.shadowmapAtlases()) {
        vkrhi::VulkanPipelineBarriers::builder()
            .insertImageMemoryBarrier(atlas.atlasImage,
                vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
                vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite
            )
            .flush(m_frameRenderingResources.commandBuffer());
    }

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::MvClear_GeometryPass));
        if (supportsPipelineStatisticsQuery) cb.beginQuery(m_pipelineStatisticsQueryPool.vkQueryPool(), 0, {});
    }

    auto albedoAndRoughnessAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.albedoAndRoughnessBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto emissiveAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.emissiveAndBloomBufferMipView(0).vkImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{ 0.0f, 0.0f, 0.0f, 0.0f });
    auto normalAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.normalBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto metallicAndAoAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.metallicAndAoBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto velocityBufferAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.velocityBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto depthAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.depthBuffer().vkImageViewWholeSize())
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
    auto positionsVertexBuffer = *MeshAssetStorage::get().meshStorage().positionsVertexBuffer().underlyingBuffer().vkBuffer();
    auto vertexBuffers = Vec<vk::Buffer>::create({
        positionsVertexBuffer,
        *MeshAssetStorage::get().meshStorage().attributesVertexBuffer().underlyingBuffer().vkBuffer()
    });
    auto indexBuffer = *MeshAssetStorage::get().meshStorage().indexBuffer().underlyingBuffer().vkBuffer();
    cb.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);

    auto uboDeviceAddr = m_frameRenderingResources.transformsBuffer().memoryDevicePtr();

    struct RenderPushConstantData {
        vk::DeviceAddress objectUniformAddr;
        vk::DeviceAddress globaldataAddr;
        vk::DeviceAddress materialDataAddr;
        vk::DeviceAddress textureToImageShaderIndexTableAddr;
        vk::DeviceAddress textureToSamplerShaderIndexTableAddr;
    };

    {
        ZoneScopedN("GeomPass - Entities Cull and Record");
        for (auto [i, renderable] : renderingData.m_renderables.m_storage.iter().enumerate()) {
            // Get the LOD list for the renderable and skip it if no LODs are available
            auto entity = renderingData.m_renderables.m_storageToEntity[i];
            auto lodLevel = m_frameRenderingResources.getLodLevel(entity.index());
            if (lodLevel == ~0u) continue;

            auto& lodList = MeshAssetStorage::get().getLodList(renderable.meshAsset);

            // Copy its transforms addr to push constants
            vk::DeviceAddress uboFinalAddr = uboDeviceAddr + i * sizeof(Transforms);

            // Pick an LOD
            Vector3f objectPos = Vector3f(0.0f);
            float objectUniformScale = 1.0f;
            ecs::Entity ent = renderingData.m_renderables.m_storageToEntity[i];
            if (renderingData.m_transforms.containsEntity(ent)) {
                auto& transformMatrix = renderingData.m_transforms.get(ent).m_transform;
                objectPos = transformMatrix.decomposePosition();
                objectUniformScale = transformMatrix.decomposeScale().max();
            }

            // See if the object is visible
            if (!isObjectVisible(objectPos, objectUniformScale * lodList.boundingSphereRadius, cameraViewMatrix, degreesToRadians(firstCamera.fov), aspectRatio, firstCamera.nearPlane, firstCamera.farPlane)) {
                continue;
            }

            auto& lod = lodList.lods[lodLevel];

            auto materialDataStride = renderable.material.shader->materialPropStructSize();
            auto materialDataAddr = m_frameRenderingResources.materialPropBuffer(materialDataStride).memoryDevicePtr() + materialDataStride * renderable.material.properties.propertiesIndex;

            auto pushconstData = RenderPushConstantData {
                .objectUniformAddr = uboFinalAddr,
                .globaldataAddr = globaldataAddr,
                .materialDataAddr = materialDataAddr,
                .textureToImageShaderIndexTableAddr = m_frameRenderingResources.textureToSrtImageIDBuffer().memoryDevicePtr(),
                .textureToSamplerShaderIndexTableAddr = m_frameRenderingResources.textureToSrtSamplerIDBuffer().memoryDevicePtr()
            };

            auto indexCount = lod.meshSuballocation.indices.size / sizeof(u32);
            auto firstIndex = lod.meshSuballocation.indices.byteOffset / sizeof(u32);
            auto vertexOffsetPositions = lod.meshSuballocation.vertexPositions.byteOffset;
            auto vertexOffsetAttributes = lod.meshSuballocation.vertexAttributes.byteOffset;
            u64 vertexBufferOffsets[] = { vertexOffsetPositions, vertexOffsetAttributes };

            cb.bindVertexBuffers(0, vertexBuffers, vertexBufferOffsets);

            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, renderable.material.shader->pipeline().vkPipeline());
            cb.pushConstants<RenderPushConstantData>(globPipelineLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, pushconstData);
            cb.drawIndexed(indexCount, 1, firstIndex, 0, 0);
            m_numDrawcalls++;
        }
    }

    cb.endRendering();

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::GeometryPass_ShadowsPass));
        if (supportsPipelineStatisticsQuery) {
            cb.endQuery(m_pipelineStatisticsQueryPool.vkQueryPool(), 0);
            cb.beginQuery(m_pipelineStatisticsQueryPool.vkQueryPool(), 1);
        }
    }

    // ---- Shadow Maps Stage ----------------------------------------------------------------------------------------------------------------------------------

    cb.bindVertexBuffers(0, positionsVertexBuffer, 0_u64);
    for (auto [i, shadowAtlasRenderingJobs] : sharedRenderingData.shadowmapAtlasRenderingJobsPerAtlas().iter().enumerate()) {
        auto depthAttachmentInfo = vk::RenderingAttachmentInfo{}
            .setImageView(sharedRenderingData.shadowmapAtlases()[i].atlasImage.vkImageViewWholeSize())
            .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearDepthStencilValue{}.setDepth(0.0f));

        auto renderArea = vk::Rect2D{}
            .setExtent({ sharedRenderingData.shadowmapAtlases()[i].atlasImage.extent().width, sharedRenderingData.shadowmapAtlases()[i].atlasImage.extent().height });

        auto renderingInfo = vk::RenderingInfo{}
            .setPDepthAttachment(&depthAttachmentInfo)
            .setLayerCount(1)
            .setRenderArea(renderArea);

        cb.beginRendering(renderingInfo);

        for (auto& shadowAtlasRenderingJob : shadowAtlasRenderingJobs) {
            auto viewport = vk::Viewport{}
                .setX(shadowAtlasRenderingJob.atlasViewportOffset.x())
                .setY(shadowAtlasRenderingJob.atlasViewportOffset.y())
                .setWidth(shadowAtlasRenderingJob.atlasViewportSize.x())
                .setHeight(shadowAtlasRenderingJob.atlasViewportSize.y())
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);
            auto scissor = vk::Rect2D{}
                .setOffset({ static_cast<i32>(shadowAtlasRenderingJob.atlasViewportOffset.x()), static_cast<i32>(shadowAtlasRenderingJob.atlasViewportOffset.y()) })
                .setExtent({ static_cast<u32>(shadowAtlasRenderingJob.atlasViewportSize.x()), static_cast<u32>(shadowAtlasRenderingJob.atlasViewportSize.y()) });

            cb.setViewport(0, viewport);
            cb.setScissor(0, scissor);
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_lightShadowRenderingPipeline.vkPipeline());


            struct RenderDepthOnlyPushConstantData {
                vk::DeviceAddress objectUniformAddr;
                vk::DeviceAddress shadowmapDescriptorAddr;
                vk::DeviceAddress globaldataAddr;
            };

            auto shadowmapDescriptorAddr = m_frameRenderingResources.shadowmapDescriptorBuffer().memoryDevicePtr() + shadowAtlasRenderingJob.shadowmapDescriptorIndex * sizeof(ShadowmapDescriptor);

            {
                ZoneScopedN("ShadowPass Job - Entities Cull and Record");
                for (auto [i, renderable] : renderingData.m_renderables.m_storage.iter().enumerate()) {
                    auto entity = renderingData.m_renderables.m_storageToEntity[i];
                    auto lodLevel = m_frameRenderingResources.getLodLevel(entity.index());
                    if (lodLevel == ~0u) continue;

                    auto& lodList = MeshAssetStorage::get().getLodList(renderable.meshAsset);

                    // Copy its transforms addr to push constants
                    vk::DeviceAddress uboFinalAddr = uboDeviceAddr + i * sizeof(Transforms);

                    // Pick an LOD
                    Vector3f objectPos = Vector3f(0.0f);
                    float objectUniformScale = 1.0f;
                    ecs::Entity ent = renderingData.m_renderables.m_storageToEntity[i];
                    if (renderingData.m_transforms.containsEntity(ent)) {
                        auto& transformMatrix = renderingData.m_transforms.get(ent).m_transform;
                        objectPos = transformMatrix.decomposePosition();
                        objectUniformScale = transformMatrix.decomposeScale().max();
                    }

                    float aspectRatio = static_cast<float>(shadowAtlasRenderingJob.atlasViewportSize.x()) / static_cast<float>(shadowAtlasRenderingJob.atlasViewportSize.y());

                    if (!isObjectVisible(objectPos, objectUniformScale * lodList.boundingSphereRadius, shadowAtlasRenderingJob.renderViewMatrix, shadowAtlasRenderingJob.renderFov, aspectRatio, shadowAtlasRenderingJob.renderNearPlane, shadowAtlasRenderingJob.randerFarPlane)) {
                        continue;
                    }

                    auto& lod = lodList.lods[lodLevel];

                    auto pushConstants = RenderDepthOnlyPushConstantData {
                        .objectUniformAddr = uboFinalAddr,
                        .shadowmapDescriptorAddr = shadowmapDescriptorAddr,
                        .globaldataAddr = m_frameRenderingResources.globalDataBuffer().memoryDevicePtr()
                    };

                    auto indexCount = lod.meshSuballocation.indices.size / sizeof(u32);
                    auto firstIndex = lod.meshSuballocation.indices.byteOffset / sizeof(u32);

                    cb.pushConstants<RenderDepthOnlyPushConstantData>(globPipelineLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, pushConstants);
                    cb.drawIndexed(indexCount, 1, firstIndex, lod.meshSuballocation.vertexPositions.byteOffset / sizeof(Vector3f), 0);
                    m_numDrawcallsShadows++;
                }
            }

        }

        cb.endRendering();
    }

    if (recordStatistics) {
        if (supportsPipelineStatisticsQuery) cb.endQuery(m_pipelineStatisticsQueryPool.vkQueryPool(), 1);
    }

    for (auto& atlas : sharedRenderingData.shadowmapAtlases()) {
        vkrhi::VulkanPipelineBarriers::builder()
            .insertImageMemoryBarrier(atlas.atlasImage,
                vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
            )
            .flush(m_frameRenderingResources.commandBuffer());
    }

    // ---- Deferred Lighting Stage ----------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.albedoAndRoughnessBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrierSubresource(renderingResources.emissiveAndBloomBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead,
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        )
        .insertImageMemoryBarrier(renderingResources.normalBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.metallicAndAoBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.velocityBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.depthBuffer(),
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.hdrColorBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    if (bloomReductionMipCount > 0) {
        vkrhi::VulkanPipelineBarriers::builder()
            .insertImageMemoryBarrierSubresource(renderingResources.emissiveAndBloomBuffer(),
                vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
                vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
                { vk::ImageAspectFlagBits::eColor, 1, bloomReductionMipCount, 0, 1 }
            )
            .flush(m_frameRenderingResources.commandBuffer());
    }

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::ShadowsPass_LightingPassAndBloom));
    }

    u32 skyboxTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingData.m_skyCubemap.index];
    u32 irradianceTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingData.m_skyIrradianceCubemap.index];
    u32 prefilterTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingData.m_skyPrefilterCubemap.index];
    u32 iblLutTextureId = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingData.m_brdfLUT.index];
    u32 linearSamplerId = renderingData.m_textureToSamplerShaderIndexSnapshot[sharedRenderingData.m_skyCubemap.index];
    u32 nearestSamplerId = TextureManager::get().samplerCache().acquireSampler(
        SamplerParams::defaultValues().setMinFilter(vk::Filter::eNearest).setMagFilter(vk::Filter::eNearest).setMipmapMode(vk::SamplerMipmapMode::eNearest).setMaxLod(0.0f)
    );

    auto drawImageAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.hdrColorBuffer().vkImageViewWholeSize())
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

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_mainLightingPassPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_mainLightingPassLayout, vk::PipelineBindPoint::eGraphics);

    struct LightingStagePushConstantData {
        vk::DeviceAddress globaldataAddr;
        vk::DeviceAddress pointlightsAddr;
        u32 pointlightCount;
        vk::DeviceAddress spotlightsAddr;
        u32 spotlightCount;
        vk::DeviceAddress shadowmapDescriptorsAddr;
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
        .globaldataAddr                  = globaldataAddr,
        .pointlightsAddr                 = m_frameRenderingResources.pointlightsBuffer().memoryDevicePtr(),
        .pointlightCount                 = static_cast<u32>(m_frameRenderingResources.pointlightsCount()),
        .spotlightsAddr                  = m_frameRenderingResources.spotlightsBuffer().memoryDevicePtr(),
        .spotlightCount                  = static_cast<u32>(m_frameRenderingResources.spotlightsCount()),
        .shadowmapDescriptorsAddr        = m_frameRenderingResources.shadowmapDescriptorBuffer().memoryDevicePtr(),
        .depthTextureIndex               = renderingResources.depthBufferIndex().imageIndex,
        .albedoAndRoughnessTextureIndex  = renderingResources.albedoAndRoughnessBufferIndex().imageIndex,
        .emissiveTextureIndex            = renderingResources.emissiveAndBloomBufferSampledImageIndexForMip(0).imageIndex,
        .normalTextureIndex              = renderingResources.normalBufferIndex().imageIndex,
        .metallicAoTextureIndex          = renderingResources.metallicAndAoBufferIndex().imageIndex,
        .skyboxTextureId                 = skyboxTextureId,
        .irradianceTextureId             = irradianceTextureId,
        .prefiltTextureId                = prefilterTextureId,
        .iblLutTextureId                 = iblLutTextureId,
        .linearSamplerIndex              = linearSamplerId,
        .nearestSamplerIndex             = nearestSamplerId
    };

    cb.pushConstants<LightingStagePushConstantData>(sharedRenderingData.m_mainLightingPassLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushconstData);
    cb.draw(3, 1, 0, 0);

    cb.endRendering();

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eColorAttachmentOutput, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::LightingPass_ColorAttOutput));
    }

    for (u32 i = 0; i < bloomReductionMipCount; i++) {
        auto srcMip = i;
        auto dstMip = i + 1;
        auto isFirstStep = i == 0;

        auto srcMipSrt = renderingResources.emissiveAndBloomBufferSampledImageIndexForMip(srcMip);
        auto dstMipSrt = renderingResources.emissiveAndBloomBufferStorageImageIndexForMip(dstMip);

        auto mipSize = math::Vector2i(renderingArea.x(), renderingArea.y()) / std::pow(2, i + 1);
        auto mipSizef = math::Vector2f(mipSize.x(), mipSize.y());

        struct BloomDownsamplePushConstants {
            math::Vector2f resolution;
            math::Vector2f texelSize;
            u32 srcMipSampledImageSrtID;
            u32 linearSamplerSrtID;
            u32 dstMipStorageImageSrtID;
            u32 isFirstMip;
        };

        auto pcData = BloomDownsamplePushConstants {
            .resolution = mipSizef,
            .texelSize = math::Vector2f::one().componentWiseDivide(mipSizef),
            .srcMipSampledImageSrtID = srcMipSrt.imageIndex,
            .linearSamplerSrtID = linearSamplerId,
            .dstMipStorageImageSrtID = dstMipSrt.imageIndex,
            .isFirstMip = static_cast<u32>(isFirstStep)
        };

        auto groupSize = math::Vector2i(16, 16);
        auto groupCount = (mipSize + groupSize - math::Vector2i::one()).componentWiseDivide(groupSize);

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, sharedRenderingData.m_bloomDownsamplePipeline.vkPipeline());
        TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_bloomDownsamplePipelineLayout, vk::PipelineBindPoint::eCompute);
        cb.pushConstants<BloomDownsamplePushConstants>(sharedRenderingData.m_bloomDownsamplePipelineLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, pcData);
        cb.dispatch(groupCount.x(), groupCount.y(), 1);

        vkrhi::VulkanPipelineBarriers::builder()
            .insertImageMemoryBarrierSubresource(renderingResources.emissiveAndBloomBuffer(),
                vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead,
                { vk::ImageAspectFlagBits::eColor, dstMip, 1, 0, 1 }
            )
            .flush(m_frameRenderingResources.commandBuffer());
    }

    for (u32 i = 0; i < bloomReductionMipCount; i++) {
        bool isFirstPass = i == 0;

        auto srcMip = renderingResources.emissiveAndBloomBuffer().mipLevels() - i - 1;
        auto dstMip = srcMip - 1;

        auto srcMipSrt = renderingResources.emissiveAndBloomBufferSampledImageIndexForMip(srcMip);
        auto dstMipSrt = renderingResources.emissiveAndBloomBufferStorageImageIndexForMip(dstMip);

        auto mipSize = math::Vector2i(renderingArea.x(), renderingArea.y()) / std::pow(2, dstMip);
        auto mipSizef = math::Vector2f(mipSize.x(), mipSize.y());
        auto groupSize = math::Vector2i(16, 16);
        auto groupCount = (mipSize + groupSize - math::Vector2i::one()).componentWiseDivide(groupSize);

        // Last mip is already in ShaderReadOnlyOptimal layout from the downsample pass, but higher mips are put into General layout as this loop iterates.
        if (!isFirstPass) {
            vkrhi::VulkanPipelineBarriers::builder()
                .insertImageMemoryBarrierSubresource(renderingResources.emissiveAndBloomBuffer(),
                    vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
                    vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead,
                    { vk::ImageAspectFlagBits::eColor, srcMip, 1, 0, 1 }
                )
                .flush(m_frameRenderingResources.commandBuffer());
        }

        // Make sure to block till the lighting pass has finished reading mip 0.
        auto srcStageMask = dstMip == 0 ? vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader : vk::PipelineStageFlagBits2::eComputeShader;
        vkrhi::VulkanPipelineBarriers::builder()
            .insertImageMemoryBarrierSubresource(renderingResources.emissiveAndBloomBuffer(),
                vk::ImageLayout::eShaderReadOnlyOptimal, srcStageMask, {},
                vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
                { vk::ImageAspectFlagBits::eColor, dstMip, 1, 0, 1}
            )
            .flush(m_frameRenderingResources.commandBuffer());

        struct BloomUpsamplePushConstants {
            math::Vector2f resolution;
            u32 smallerMipSampledImageSrtID;
            u32 linearSamplerSrtID;
            u32 dstMipStorageImageSrtID;
            float filterRadius;
        };

        auto pcData = BloomUpsamplePushConstants {
            .resolution = mipSizef,
            .smallerMipSampledImageSrtID = srcMipSrt.imageIndex,
            .linearSamplerSrtID = linearSamplerId,
            .dstMipStorageImageSrtID = dstMipSrt.imageIndex,
            .filterRadius = cvRdBloomRadius.get()
        };

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, sharedRenderingData.m_bloomUpsamplePipeline.vkPipeline());
        TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_bloomUpsamplePipelineLayout, vk::PipelineBindPoint::eCompute);
        cb.pushConstants<BloomUpsamplePushConstants>(sharedRenderingData.m_bloomUpsamplePipelineLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, pcData);
        cb.dispatch(groupCount.x(), groupCount.y(), 1);

        if (dstMip == 0) {
            vkrhi::VulkanPipelineBarriers::builder()
                .insertImageMemoryBarrierSubresource(renderingResources.emissiveAndBloomBuffer(),
                    vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
                    vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead,
                    { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
                )
                .flush(m_frameRenderingResources.commandBuffer());
        }
    }

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eComputeShader, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::Bloom_ComputeShader));
    }

    // ---- Tonemapped Frame Fuse ------------------------------------------------------------------------------------------------------------------------------

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.hdrColorBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.tonemappedColorBuffer(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::LightingPassAndBloom_TonemapFuse));
    }

    auto dstTonemappedFrameAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.tonemappedColorBuffer().vkImageViewWholeSize())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto tonemappedFrameFuseRenderingInfo = vk::RenderingInfo{}
        .setColorAttachments(dstTonemappedFrameAttachmentInfo)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{}.setExtent(vkRenderingArea));

    cb.beginRendering(tonemappedFrameFuseRenderingInfo);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_tonemappedFrameFusePipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_tonemappedFrameFuseLayout, vk::PipelineBindPoint::eGraphics);

    struct TonemappedFrameFusePushConstants {
        vk::DeviceAddress globaldataAddr;
        u32 nearestSamplerSrtID;
        u32 hdrColorBufferSrtID;
        u32 filteredBloomImageSrtID;
        float bloomStrength;
    } __attribute((packed));

    auto tonemappedFrameFusePcData = TonemappedFrameFusePushConstants {
        .globaldataAddr = globaldataAddr,
        .nearestSamplerSrtID = nearestSamplerId,
        .hdrColorBufferSrtID = renderingResources.hdrColorBufferIndex().imageIndex,
        .filteredBloomImageSrtID = renderingResources.emissiveAndBloomBufferSampledImageIndexForMip(0).imageIndex,
        .bloomStrength = cvRdBloomStrength.get()
    };

    cb.pushConstants<TonemappedFrameFusePushConstants>(sharedRenderingData.m_tonemappedFrameFuseLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, tonemappedFrameFusePcData);
    cb.draw(3, 1, 0, 0);

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

    u32 smaaAreaTextureSrtID = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingData.m_smaaAreaTexture.index];
    u32 smaaSearchTextureSrtID = renderingData.m_textureToImageShaderIndexSnapshot[sharedRenderingData.m_smaaSearchTexture.index];

    // -------- [Stage 1] Edge detection

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.tonemappedColorBuffer(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.smaaEdgesImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::TonemapFuse_Smaa));
    }

    auto edgesImageAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.smaaEdgesImage().vkImageViewWholeSize())
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

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_smaaEdgeDetectPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_smaaEdgeDetectLayout, vk::PipelineBindPoint::eGraphics);

    struct SmaaEdgeDetectionPushConstantData {
        Vector4f rtMetrics;
        u32 colorBufferSrtID;
        u32 linearSamplerSrtID;
        u32 nearestSamplerSrtID;
    };

    auto smaaEdgeDetectionPushconstData = SmaaEdgeDetectionPushConstantData {
        .rtMetrics = smaaRtMetrics,
        .colorBufferSrtID = renderingResources.tonemappedColorBufferUnormViewIndex().imageIndex,
        .linearSamplerSrtID = smaaLinearSamplerSrtID,
        .nearestSamplerSrtID = smaaNearestSamplerSrtID
    };

    cb.pushConstants<SmaaEdgeDetectionPushConstantData>(sharedRenderingData.m_smaaEdgeDetectLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaEdgeDetectionPushconstData);
    cb.draw(3, 1, 0, 0);

    cb.endRendering();

    // -------- [Stage 2] Blend weights

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.smaaEdgesImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.smaaWeightsImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eFragmentShader, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto weightsImageAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.smaaWeightsImage().vkImageViewWholeSize())
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

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_smaaBlendWeightPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_smaaBlendWeightLayout, vk::PipelineBindPoint::eGraphics);

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
        .edgesImageSrtID = renderingResources.smaaEdgesImageIndex().imageIndex,
        .areaTextureSrtID = smaaAreaTextureSrtID,
        .searchTextureSrtID = smaaSearchTextureSrtID,
        .linearSamplerSrtID = smaaLinearSamplerSrtID,
        .nearestSamplerSrtID = smaaNearestSamplerSrtID,
        .frameIndex = static_cast<u32>(renderingData.m_frameIndex)
    };

    cb.pushConstants<SmaaBlendWeightPushConstantData>(sharedRenderingData.m_smaaBlendWeightLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaBlendWeightPushconstData);
    cb.draw(3, 1, 0, 0);

    cb.endRendering();

    // -------- [Stage 3] Neighborhood blend

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.smaaWeightsImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.postSmaaImage(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eBlit, {},
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    auto finalDrawBufferUnormAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.postSmaaImage().vkImageViewWholeSize())
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

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_smaaNeighborhoodBlendPipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_smaaNeighborhoodBlendLayout, vk::PipelineBindPoint::eGraphics);

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
        .colorBufferSrtID = renderingResources.tonemappedColorBufferIndex().imageIndex,
        .weightsImageSrtID = renderingResources.smaaWeightsImageIndex().imageIndex,
        .velocityBufferSrtID = renderingResources.velocityBufferIndex().imageIndex,
        .linearSamplerSrtID = smaaLinearSamplerSrtID,
        .nearestSamplerSrtID = smaaNearestSamplerSrtID
    };

    cb.pushConstants<SmaaNeighborhoodBlendPushConstantData>(sharedRenderingData.m_smaaNeighborhoodBlendLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaNeighborhoodBlendPushconstData);
    cb.draw(3, 1, 0, 0);

    cb.endRendering();

    // -------- [Stage 4] Temporal resolve

    auto frameParity = (renderingData.m_frameIndex & 1) == 0;
    auto& currentColorBuffer = frameParity ? renderingResources.smaaColorResolvedBuffer0() : renderingResources.smaaColorResolvedBuffer1();
    auto& currentColorBufferUnorm = frameParity ? renderingResources.smaaColorResolvedBuffer0UnormView() : renderingResources.smaaColorResolvedBuffer1UnormView();

    auto smaaNeighborhoodResolvedBufferIndex = renderingResources.postSmaaImageUnormViewIndex();
    auto lastColorBufferIndex = frameParity ? renderingResources.smaaColorResolvedBuffer1UnormViewIndex() : renderingResources.smaaColorResolvedBuffer0UnormViewIndex();

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.postSmaaImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(renderingResources.finalImage(),
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
        .setImageView(renderingResources.finalImageUnormView().vkImageView())
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

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_smaaTemporalResolvePipeline.vkPipeline());
    TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_smaaTemporalResolveLayout, vk::PipelineBindPoint::eGraphics);

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
        .colorTextureSrtID = renderingResources.postSmaaImageUnormViewIndex().imageIndex,
        .lastResolvedColorTextureSrtID = lastColorBufferIndex.imageIndex,
        .velocityBufferSrtID = renderingResources.velocityBufferIndex().imageIndex,
        .smaaLinearSamplerSrtID = smaaLinearSamplerSrtID,
        .smaaNearestSamplerSrtID = smaaNearestSamplerSrtID
    };

    cb.pushConstants<SmaaTemporalResolvePushConstants>(sharedRenderingData.m_smaaTemporalResolveLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, smaaTemporalResolvePushConstants);
    cb.draw(3, 1, 0, 0);
    cb.endRendering();

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
        .insertImageMemoryBarrier(renderingResources.finalImage(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite
        )
        .flush(m_frameRenderingResources.commandBuffer());

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::Smaa_UI));
    }

    auto finalDrawBufferAttachmentInfo = vk::RenderingAttachmentInfo{}
        .setImageView(renderingResources.finalImage().vkImageViewWholeSize())
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

                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_uiRectRendererPipeline.vkPipeline());
                cb.pushConstants<PushConstants>(sharedRenderingData.m_uiRectRendererLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
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
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_uiTextureRendererPipeline.vkPipeline());
                TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_uiTextureRendererLayout, vk::PipelineBindPoint::eGraphics);
                cb.pushConstants<PushConstants>(sharedRenderingData.m_uiTextureRendererLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
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
                    .instanceBuffer = textInstanceBuffers.last().memoryDevicePtr(),
                    .ssGlobalOffset = drawCmd.ssPosition.round(),
                    .screenSize = renderingArea,
                    .samplerSrtID = sampler2,
                    .color = drawCmd.color.asRgba32Float()
                };

                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sharedRenderingData.m_bitmapFontRendererPipeline.vkPipeline());
                TextureManager::get().shaderResourceTable().bindToCommandBuffer(m_frameRenderingResources.commandBuffer(), sharedRenderingData.m_bitmapFontRendererLayout, vk::PipelineBindPoint::eGraphics);

                cb.pushConstants<PushConstants>(sharedRenderingData.m_bitmapFontRendererLayout.vkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
                cb.draw(4, shapedText.size(), 0, 0);
            }
        );
    }

    cb.endRendering();

    if (recordStatistics) {
        cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, m_timestampsQueryPool.vkQueryPool(), static_cast<u32>(FrameContextTimestampIndex::UI_CbFinish));
    }

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(renderingResources.finalImage(),
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

    auto srcOffsets = std::array<vk::Offset3D, 2>{{{0, 0, 0}, {toOffset3D(renderingResources.postSmaaImage().extent())}}};

    auto blit = vk::ImageBlit2{}
        .setSrcSubresource(blitSubres)
        .setDstSubresource(blitSubres)
        .setSrcOffsets(srcOffsets)
        .setDstOffsets(srcOffsets);

    auto blitInfo = vk::BlitImageInfo2{}
        .setSrcImage(renderingResources.finalImage().vkImage())
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

    {
        ZoneScopedN("CB Submit")
        vkrhi::VulkanContext::get().vkQueueGraphics().submitOneCommandBufferWithBinarySemaphores(
            cb,
            {}, {},
            m_frameRenderingResources.imageAcquiredSemaphore(), swapchainImage.vkSemaphoreImagePresent(),
            vk::PipelineStageFlagBits2::eBlit, vk::PipelineStageFlagBits2::eBlit,
            Some(std::ref(m_frameRenderingResources.frameDoneFence()))
        );
    }

    {
        ZoneScopedN("AntiLag Present Pace");
        vkrhi::VulkanContext::get().antiLagPacePresent(renderingData.m_frameIndex, 0);
    }
    vk::Result presentResult;
    {
        ZoneScopedN("Present");
        presentResult = vkrhi::VulkanContext::get().vkQueuePresent().submitPresent(swapchain, swapchainImage.vkSemaphoreImagePresent(), imageAcquire.first.unwrap());
    }

    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        return { .shouldRecreateSwapchain = true, .stepPerFrameResources = true };
    }

    return { .shouldRecreateSwapchain = shouldRecreateSwapchainAfter, .stepPerFrameResources = true };
}

} // namespace projnekomata