module projnekomata;
import vulkan;
import projnekomata.corelib;
import vk_mem_alloc;
import :graphics.vulkan.context;
import :graphics.rendering.transient_rendering_resources;
import :graphics.cmd_alloc;
import :graphics.vulkan.vk_commands_barriers;

namespace projnekomata::gfx {

TransientRenderingResources::TransientRenderingResources(std::nullptr_t) {  }

TransientRenderingResources::TransientRenderingResources(vk::Extent2D renderImageExtent, SharedRenderingResources& sharedResources) {
    auto& srt = TextureManager::get().shaderResourceTable();
    m_depthBufferIndex = srt.allocateSampledImageIndex();
    m_albedoAndRoughnessBufferIndex = srt.allocateSampledImageIndex();
    m_normalBufferIndex = srt.allocateSampledImageIndex();
    m_metallicAndAoBufferIndex = srt.allocateSampledImageIndex();
    m_velocityBufferIndex = srt.allocateSampledImageIndex();
    m_smaaEdgesImageIndex = srt.allocateSampledImageIndex();
    m_smaaWeightsImageIndex = srt.allocateSampledImageIndex();
    m_colorBufferIndex = srt.allocateSampledImageIndex();
    m_colorBufferUnormViewIndex = srt.allocateSampledImageIndex();

    m_smaaColorResolvedBuffer0UnormViewIndex = srt.allocateSampledImageIndex();
    m_smaaColorResolvedBuffer1UnormViewIndex = srt.allocateSampledImageIndex();

    m_postSmaaImageIndex = srt.allocateSampledImageIndex();
    m_postSmaaImageUnormViewIndex = srt.allocateSampledImageIndex();

    m_overdrawCountersImageIndex = srt.allocateStorageImageIndex();

    setupRenderingAttachments(renderImageExtent);
}

auto TransientRenderingResources::handleWindowSizeChange(vk::Extent2D newWindowSize) -> void {
    setupRenderingAttachments(newWindowSize);
}

auto renderTargetImageBuilderPrefab(vk::Extent2D renderImageExtent) -> vkrhi::VulkanImageBuilder {
    return vkrhi::VulkanImage::builder()
        .type(vk::ImageType::e2D)
        .extentsrd(vk::Extent3D { renderImageExtent, 1 }, 1, 1)
        .isCubemap(false)
        .tiling(vk::ImageTiling::eOptimal)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
        .initialLayout(vk::ImageLayout::eUndefined);
}

auto TransientRenderingResources::setupRenderingAttachments(vk::Extent2D renderImageExtent) -> void {
    auto affectedQueues = vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_queueFamilies[vkrhi::QueueFamily::Graphics];
    auto& srt = TextureManager::get().shaderResourceTable();

    auto colorMutableFormats = StaticSlice<const vk::Format>::inst<vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Unorm>();

    m_depthBuffer = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("Depth Buffer")
        .format(vk::Format::eD32Sfloat)
        .usage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
        .build();

    m_albedoAndRoughnessBuffer = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("G-Buffer Albedo and Roughness Buffer")
        .format(vk::Format::eR8G8B8A8Srgb)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .build();

    m_normalBuffer = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("G-Buffer Normal Buffer")
        .format(vk::Format::eR16G16B16A16Snorm)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .build();

    m_metallicAndAoBuffer = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("G-Buffer Metallic and AO Buffer")
        .format(vk::Format::eR8G8Unorm)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .build();

    m_velocityBuffer = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("Velocity Buffer")
        .format(vk::Format::eR16G16Sfloat)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .build();

    m_colorBuffer = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("Color Buffer")
        .format(vk::Format::eR8G8B8A8Srgb)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .mutableFormat(colorMutableFormats)
        .build();

    m_colorBufferUnormView = m_colorBuffer.createImageViewWithFormat(vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1, false);

    m_smaaColorResolvedBuffer0 = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("SMAA Color Resolved Buffer 0")
        .format(vk::Format::eR8G8B8A8Srgb)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
        .mutableFormat(colorMutableFormats)
        .build();

    m_smaaColorResolvedBuffer1 = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("SMAA Color Resolved Buffer 1")
        .format(vk::Format::eR8G8B8A8Srgb)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
        .mutableFormat(colorMutableFormats)
        .build();

    m_smaaColorResolvedBuffer0UnormView = m_smaaColorResolvedBuffer0.createImageViewWithFormat(vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1, false);
    m_smaaColorResolvedBuffer1UnormView = m_smaaColorResolvedBuffer1.createImageViewWithFormat(vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1, false);

    m_smaaEdgesImage = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("SMAA Edges Image")
        .format(vk::Format::eR8G8Unorm)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .build();

    m_smaaWeightsImage = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("SMAA Weights Image")
        .format(vk::Format::eR8G8B8A8Unorm)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .build();

    m_postSmaaImage = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("Post SMAA Image")
        .format(vk::Format::eR8G8B8A8Srgb)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .mutableFormat(colorMutableFormats)
        .build();

    m_postSmaaImageUnormView = m_postSmaaImage.createImageViewWithFormat(vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1, false);

    m_finalImage = renderTargetImageBuilderPrefab(renderImageExtent)
        .name("Final Image")
        .format(vk::Format::eR8G8B8A8Srgb)
        .usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc)
        .mutableFormat(colorMutableFormats)
        .build();

    m_finalImageUnormView = m_finalImage.createImageViewWithFormat(vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1, false);

    vk::Extent2D halfExtentCeil = vk::Extent2D {
        (renderImageExtent.width + 1) / 2,
        (renderImageExtent.height + 1) / 2,
    };

    m_overdrawCountersImage = renderTargetImageBuilderPrefab(halfExtentCeil)
        .format(vk::Format::eR32Uint)
        .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst)
        .build();

    srt.bindSampledImage(m_depthBuffer, m_depthBufferIndex);
    srt.bindSampledImage(m_albedoAndRoughnessBuffer, m_albedoAndRoughnessBufferIndex);
    srt.bindSampledImage(m_normalBuffer, m_normalBufferIndex);
    srt.bindSampledImage(m_metallicAndAoBuffer, m_metallicAndAoBufferIndex);
    srt.bindSampledImage(m_velocityBuffer, m_velocityBufferIndex);
    srt.bindSampledImage(m_smaaEdgesImage, m_smaaEdgesImageIndex);
    srt.bindSampledImage(m_smaaWeightsImage, m_smaaWeightsImageIndex);
    srt.bindSampledImage(m_colorBuffer, m_colorBufferIndex);
    srt.bindSampledImageView(m_colorBufferUnormView, m_colorBufferUnormViewIndex);
    srt.bindSampledImageView(m_smaaColorResolvedBuffer0UnormView, m_smaaColorResolvedBuffer0UnormViewIndex);
    srt.bindSampledImageView(m_smaaColorResolvedBuffer1UnormView, m_smaaColorResolvedBuffer1UnormViewIndex);
    srt.bindSampledImage(m_postSmaaImage, m_postSmaaImageIndex);
    srt.bindSampledImageView(m_postSmaaImageUnormView, m_postSmaaImageUnormViewIndex);

    srt.bindStorageImage(m_overdrawCountersImage, m_overdrawCountersImageIndex);

    zeroinitColorBuffers();
}
auto TransientRenderingResources::zeroinitColorBuffers() -> void {
    auto cb = vkrhi::VulkanCommandPoolsList::getAssignedGraphicsCommandPool().allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
    auto& cmd = cb.vkCommandBuffer();

    auto beginInfo = vk::CommandBufferBeginInfo{}
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vkrhi::vkCheckResult(cmd.begin(beginInfo));

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(m_smaaColorResolvedBuffer0,
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
        )
        .insertImageMemoryBarrier(m_smaaColorResolvedBuffer1,
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
        )
        .flush(cb);

    cmd.clearColorImage(m_smaaColorResolvedBuffer0.vkImage(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue { 0.0f, 0.0f, 0.0f, 0.0f }, m_smaaColorResolvedBuffer0.subresourceRangeFull());
    cmd.clearColorImage(m_smaaColorResolvedBuffer1.vkImage(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue { 0.0f, 0.0f, 0.0f, 0.0f }, m_smaaColorResolvedBuffer1.subresourceRangeFull());

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(m_smaaColorResolvedBuffer0,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .insertImageMemoryBarrier(m_smaaColorResolvedBuffer1,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
        )
        .flush(cb);

    vkrhi::vkCheckResult(cmd.end());
    vkrhi::VulkanContext::get().vkQueueGraphics().submitOneCommandBuffer(cmd, {},{}, None)
        .await();
}

} // namespace projnekomata