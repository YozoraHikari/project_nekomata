module;
#include <vulkan/vulkan.h>
#include <ktxvulkan.h>
#include <cstring>
module projnekomata;
import fmt;
import vk_mem_alloc;
import projnekomata.corelib;
import :graphics.cmd_alloc;
import :graphics.vulkan.vk_queue_family_swizzling;
import :graphics.vulkan.vk_buffer;
import :graphics.vulkan.vk_commands_barriers;
import :graphics.srt.bindless_descriptor_set_srt;
import :graphics.vulkan.context;
import :graphics.texturesystem.texture_manager;
import :core.fs.path_resolve;

namespace projnekomata::gfx {

TextureResources::TextureResources(std::nullptr_t) {}
TextureResources::TextureResources(vkrhi::VulkanImage&& image)
    : m_image(std::move(image)) {}

TextureManager::TextureManager(std::nullptr_t) {}
TextureManager::TextureManager(Unique<IShaderResourceTable>&& srt)
    : m_loadedTextures(VaSlotmap<TextureResources, kMaxTextureCount>::create()), m_textureToShaderIndexTable(kMaxTextureCount), m_srt(std::move(srt)) {}

auto TextureManager::create() -> Unique<TextureManager> {
    debug_assert(g_textureManager == nullptr, "only one TextureManager may live at any given time");

    auto srt = BindlessDescriptorSetShaderResourceTable::create(kMaxSampledImageCount, 2048, kMaxSamplerCount);

    auto textureManager = Unique<TextureManager>::create(Unique<IShaderResourceTable>::upcast(std::move(srt)));
    g_textureManager = textureManager.ptr();
    auto pxData = Vec<u8>::create({0x39, 0x43, 0x52, 0xff});
    // loadTextureFromMemoryInternal depends on the sampler!!!
    u32 defaultSamplerIndex = g_textureManager->m_samplerCache.acquireSampler(SamplerParams::defaultValues());
    debug_assert(defaultSamplerIndex == 0, "the default sampler didn't have index 0");
    g_textureManager->m_defaultTexture = g_textureManager->loadTextureFromMemoryInternal(1, 1, 1, 1, 1, vk::Format::eR8G8B8A8Srgb, pxData.asSlice(), SamplerParams::defaultValues());
    debug_assert(g_textureManager->m_defaultTexture.index == 0, "the default texture didn't have index 0");
    return textureManager;
}
auto TextureManager::createTexture(u32 width, u32 height, u32 depth, u32 layers, u32 mipLevels, bool isCube, vk::Format format, vk::ImageUsageFlags usage,
                                   const SamplerParams& samplerParams) -> Texture {
    // TODO: fix UB with assigning 2D null image when image is a type other than 2D
    auto image = vkrhi::VulkanImage::builder()
        .type(vk::ImageType::e2D)
        .extentsrd(vk::Extent3D { width, height, depth }, layers, mipLevels)
        .isCubemap(isCube)
        .format(format)
        .tiling(vk::ImageTiling::eOptimal)
        .usage(usage)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics | vkrhi::QueueFamily::AsyncCompute)
        .initialLayout(vk::ImageLayout::eUndefined)
        .build();

    auto texture = allocateTexture(std::move(image));
    auto imageShaderIndex = m_srt->allocateSampledImageIndex();
    auto samplerShaderIndex = m_samplerCache.acquireSampler(samplerParams);
    TextureManager::get().m_srt->bindSampledImage(getTextureResources(texture).image(), imageShaderIndex);
    m_textureToShaderIndexTable.setTextureShaderImageIndex(texture.index, imageShaderIndex.imageIndex);
    m_textureToShaderIndexTable.setTextureShaderSamplerIndex(texture.index, samplerShaderIndex);
    return texture;
}

auto TextureManager::loadTextureFromMemoryInternal(u32 width, u32 height, u32 depth, u32 arrayLayers, u32 mipLevels, vk::Format format,
                                           Slice<const u8> data, const SamplerParams& samplerParams) -> Texture {
    auto image = vkrhi::VulkanImage::builder()
        .type(vk::ImageType::e2D)
        .extentsrd(vk::Extent3D { width, height, depth }, arrayLayers, mipLevels)
        .isCubemap(false)
        .format(format)
        .tiling(vk::ImageTiling::eOptimal)
        .usage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics | vkrhi::QueueFamily::AsyncCompute)
        .initialLayout(vk::ImageLayout::eUndefined)
        .build();

    auto buffer = vkrhi::VulkanBuffer::builder()
        .len(data.size())
        .usage(vk::BufferUsageFlagBits::eTransferSrc)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics | vkrhi::QueueFamily::AsyncCompute)
        .build();

    memcpy(buffer.memoryHostPtr(), data.data(), data.size());
    auto cmd = vkrhi::VulkanCommandPoolsList::getAssignedAsyncComputeCommandPool().allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
    auto& cb = cmd.vkCommandBuffer();
    auto cbBeginInfo = vk::CommandBufferBeginInfo{}
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vkrhi::vkCheckResult(cb.begin(cbBeginInfo));
    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(
            image,
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
        )
        .flush(cmd);

    auto copyRegion = vk::BufferImageCopy2{}
        .setImageExtent(vk::Extent3D { width, height, depth })
        .setImageOffset(vk::Offset3D { 0, 0, 0 })
        .setImageSubresource(vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
        .setBufferOffset(0)
        .setBufferImageHeight(0)
        .setBufferRowLength(0);
    auto copyInfo = vk::CopyBufferToImageInfo2{}
        .setDstImage(image.vkImage())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setSrcBuffer(buffer.vkBuffer())
        .setRegions(copyRegion);
    cb.copyBufferToImage2(copyInfo);
    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(
            image,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone
        )
        .flush(cmd);
    vkrhi::vkCheckResult(cb.end());
    auto asyncOp = vkrhi::VulkanContext::get().vkQueueAsyncCompute().submitOneCommandBuffer(cb, {}, {}, None);
    asyncOp.await();

    auto texture = allocateTexture(std::move(image));
    auto imageShaderIndex = m_srt->allocateSampledImageIndex();
    auto samplerShaderIndex = m_samplerCache.acquireSampler(samplerParams);
    TextureManager::get().m_srt->bindSampledImage(getTextureResources(texture).image(), imageShaderIndex);
    m_textureToShaderIndexTable.setTextureShaderImageIndex(texture.index, imageShaderIndex.imageIndex);
    m_textureToShaderIndexTable.setTextureShaderSamplerIndex(texture.index, samplerShaderIndex);

    return texture;
}

auto TextureManager::loadKtx2TextureAsync(const fs::Path& path, const SamplerParams& samplerParams) -> Texture {
    auto resolvedPath = fs::PathResolver::resolve(path);

    ktxTexture2* ktxData;
    ktx_error_code_e result = ktxTexture2_CreateFromNamedFile(resolvedPath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxData);

    if (result != KTX_SUCCESS) {
        panic("failed to load texture '{}': {}", path.string(), ktxErrorString(result));
    }
    
    auto texture = allocateTexture(nullptr);
    
    auto thr = std::thread(uploadKtx2Image, path.string(), texture, ktxData, samplerParams);
    thr.detach();
    
    // thread runs ktxTexture2_Destroy

    return texture;
}
auto TextureManager::loadKtx2TextureBlocking(const fs::Path& path, const SamplerParams& samplerParams) -> Texture {
    auto resolvedPath = fs::PathResolver::resolve(path);

    ktxTexture2* ktxData;
    ktx_error_code_e result = ktxTexture2_CreateFromNamedFile(resolvedPath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxData);

    if (result != KTX_SUCCESS) {
        panic("failed to load texture '{}': {}", path.string(), ktxErrorString(result));
    }

    auto texture = allocateTexture(nullptr);

    uploadKtx2Image(path.string(), texture, ktxData, samplerParams);

    // the function runs ktxTexture2_Destroy

    return texture;
}

auto TextureManager::laodKtx2TextureFromMemoryBlocking(std::string name, Slice<const u8> data, const SamplerParams& samplerParams) -> Texture {
    ktxTexture2* ktxData;
    ktx_error_code_e result = ktxTexture2_CreateFromMemory(data.data(), data.len(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxData);
    if (result != KTX_SUCCESS) {
        panic("failed to load texture '{}' from memory: {}", name, ktxErrorString(result));
    }

    auto texture = allocateTexture(nullptr);

    uploadKtx2Image(std::move(name), texture, ktxData, samplerParams);

    // the function runs ktxTexture2_Destroy

    return texture;
}

auto TextureManager::allocateTexture(vkrhi::VulkanImage&& img) -> Texture {
    auto textureIndex = m_loadedTextures.emplace(std::move(img));
    return Texture(textureIndex);
}

auto TextureManager::freeTexture(Texture texture) -> void {
    log::warn("some texture just got leaked");
}

auto pickTranscodeDst(u32 numChannels, bool isHdr) -> ktx_transcode_fmt_e {
    auto& physdevProps = vkrhi::VulkanContext::get().vkPhysicalDeviceProps();

    if (isHdr) {
        if (physdevProps.m_textureFormatSupportASTCHDR) return KTX_TTF_ASTC_HDR_4x4_RGBA;
        if (physdevProps.m_textureFormatSupportBC6HU) return KTX_TTF_BC6HU;
        return KTX_TTF_RGBA_HALF;
    }

    switch (numChannels) {
        case 4:
        case 3: {
            if (physdevProps.m_textureFormatSupportASTC) return KTX_TTF_ASTC_4x4_RGBA;
            if (physdevProps.m_textureFormatSupportBC7) return KTX_TTF_BC7_RGBA;
            if (physdevProps.m_textureFormatSupportETC2) return KTX_TTF_ETC2_RGBA;
            return KTX_TTF_RGBA32;
        }
        case 2: {
            if (physdevProps.m_textureFormatSupportASTC) return KTX_TTF_ASTC_4x4_RGBA;
            if (physdevProps.m_textureFormatSupportEAC) return KTX_TTF_ETC2_EAC_RG11;
            if (physdevProps.m_textureFormatSupportBC5) return KTX_TTF_BC5_RG;
            return KTX_TTF_RGBA32;
        }
        case 1: {
            if (physdevProps.m_textureFormatSupportEAC) return KTX_TTF_ETC2_EAC_R11;
            if (physdevProps.m_textureFormatSupportBC4) return KTX_TTF_BC4_R;
            if (physdevProps.m_textureFormatSupportASTC) return KTX_TTF_ASTC_4x4_RGBA;
            return KTX_TTF_RGBA32;
        }
        default: panic("unsupported number of channels in loaded texture");
    }
}

auto TextureManager::uploadKtx2Image(std::string name, Texture texture, ktxTexture2* ktxData, const SamplerParams& samplerParams) -> void {
    vkrhi::VulkanCommandPoolsList::initThreadLocalCommandPools();

    bool needsTranscoding = ktxTexture2_IsTranscodable(ktxData);
    if (needsTranscoding) {
        auto numComponents = ktxTexture2_GetNumComponents(ktxData);
        auto isHdr = ktxTexture2_IsHDR(ktxData);
        ktx_transcode_fmt_e transcodeFormat = pickTranscodeDst(numComponents, isHdr);
        ktx_error_code_e transcodeResult = ktxTexture2_TranscodeBasis(ktxData, transcodeFormat, 0);
        if (transcodeResult != KTX_SUCCESS) {
            panic("failed to transcode texture '{}': {}", name, ktxErrorString(transcodeResult));
        }
    }

    auto& textureResources = TextureManager::get().getTextureResources(texture);

    // Data parameters
    auto bufferSize = ktxData->dataSize;
    auto imgData = ktxData->pData;

    // Image extents and mip chain
    auto imageWidth = ktxData->baseWidth;
    auto imageHeight = ktxData->baseHeight;
    auto imageDepth = ktxData->baseDepth;
    auto imageArrayLayers = ktxData->numLayers > 0 ? ktxData->numLayers : 1;
    auto imageMipLevels = ktxData->numLevels;
    auto imageIsCubemap = ktxData->isCubemap;

    // Image usage and format
    auto imageFormat = static_cast<vk::Format>(ktxTexture2_GetVkFormat(ktxData));

    // Vulkan Image
    auto dname = fmt::format("Texture: \"{}\"", name);
    auto image = vkrhi::VulkanImage::builder()
        .name(dname)
        .type(vk::ImageType::e2D)
        .extentsrd(vk::Extent3D { imageWidth, imageHeight, imageDepth }, imageArrayLayers, imageMipLevels)
        .isCubemap(imageIsCubemap)
        .format(imageFormat)
        .tiling(vk::ImageTiling::eOptimal)
        .usage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics | vkrhi::QueueFamily::AsyncCompute)
        .initialLayout(vk::ImageLayout::eUndefined)
        .build();

    log::info("Texture {} size: {}x{}x{}{} mips: {} layers: {} mem: {} format: {}{}",
        name,
        imageWidth, imageHeight, imageDepth,
        imageIsCubemap ? " cubemap" : "",
        imageMipLevels, imageArrayLayers,
        bufferSize >= 1048576 ? fmt::format("{} MiB", bufferSize / 1048576) : fmt::format("{} kiB", bufferSize / 1024),
        vk::to_string(imageFormat), needsTranscoding ? " decompr" : ""
    );

    // Staging buffer
    auto buffer = vkrhi::VulkanBuffer::builder()
        .len(bufferSize)
        .usage(vk::BufferUsageFlagBits::eTransferSrc)
        .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
        .memoryMapping(vkrhi::VulkanBufferMemoryMapping::MapForSequentialWrite)
        .memoryRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        .queueFamilyIndices(vkrhi::QueueFamily::Graphics | vkrhi::QueueFamily::AsyncCompute)
        .build();

    memcpy(buffer.memoryHostPtr(), imgData, bufferSize);

    // Command to upload
    auto cmd = vkrhi::VulkanCommandPoolsList::getAssignedAsyncComputeCommandPool().allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
    auto& cb = cmd.vkCommandBuffer();

    auto cbBeginInfo = vk::CommandBufferBeginInfo{}
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    vkrhi::vkCheckResult(cb.begin(cbBeginInfo));
    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(
            image,
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
        )
        .flush(cmd);

    Vec<vk::BufferImageCopy2> copyRegions;

    u32 faceCount = imageIsCubemap ? 6 : 1;

    for (u32 mip = 0; mip < imageMipLevels; mip++) {
        for (u32 layer = 0; layer < imageArrayLayers; layer++) {
            for (u32 face = 0; face < faceCount; face++) {
                u32 arrayLayer = imageIsCubemap ? (layer * 6) + face : layer;

                auto subresLayers = vk::ImageSubresourceLayers{}
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseArrayLayer(arrayLayer)
                    .setLayerCount(1)
                    .setMipLevel(mip);

                ktx_size_t offset;
                ktxTexture2_GetImageOffset(ktxData, mip, layer, face, &offset);

                auto extent = vk::Extent3D{
                    std::max(1u,  imageWidth >> mip),
                    std::max(1u, imageHeight >> mip),
                    std::max(1u,  imageDepth >> mip)
                };

                auto copyRegion = vk::BufferImageCopy2{}
                    .setImageExtent(extent)
                    .setImageOffset(vk::Offset3D{0, 0, 0})
                    .setImageSubresource(subresLayers)
                    .setBufferOffset(offset)
                    .setBufferImageHeight(0)
                    .setBufferRowLength(0);
                copyRegions.emplace(copyRegion);
            }
        }
    }

    auto copyInfo = vk::CopyBufferToImageInfo2{}
        .setDstImage(image.vkImage())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setSrcBuffer(buffer.vkBuffer())
        .setRegions(copyRegions);

    cb.copyBufferToImage2(copyInfo);

    vkrhi::VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(
            image,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone
        )
        .flush(cmd);
    vkrhi::vkCheckResult(cb.end());

    auto future = vkrhi::VulkanContext::get().vkQueueAsyncCompute().submitOneCommandBuffer(cb, {}, {}, None);
    ktxTexture2_Destroy(ktxData);

    // TODO: Move somewhere else. We don't want it blocking an entire job when a threadpool gets in place.
    textureResources.setImage(std::move(image));
    // Wait for the upload to complete and then let the texture be used for rendering
    future.await();
    auto imageShaderIndex = TextureManager::get().m_srt->allocateSampledImageIndex();
    TextureManager::get().m_srt->bindSampledImage(textureResources.image(), imageShaderIndex);
    TextureManager::get().m_textureToShaderIndexTable.setTextureShaderImageIndex(texture.index, imageShaderIndex.imageIndex);
    TextureManager::get().m_textureToShaderIndexTable.setTextureShaderSamplerIndex(texture.index, TextureManager::get().m_samplerCache.acquireSampler(samplerParams));
}

} // namespace projnekomata::texturesystem