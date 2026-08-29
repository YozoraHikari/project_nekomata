module;
#include <xxhash.h>
export module projnekomata:graphics.fontsystem.dynamic_font_atlas;
import std;
import fmt;
import projnekomata.corelib;
import vulkan;
import vk_mem_alloc;
import :core.math;
import :graphics.fontsystem.font_face;
import :graphics.vulkan.vk_image;
import :graphics.texturesystem.texture_manager;
import :graphics.vulkan.vk_queue_family_swizzling;
import :graphics.vulkan.context;

export namespace projnekomata::gfx {

class AtlasShelfPacker {
public:
    AtlasShelfPacker(std::nullptr_t);
    AtlasShelfPacker(i32 width, i32 height);

    Option<math::Vector2i> pack(i32 width, i32 height);

private:
    struct Shelf {
        i32 height;
        i32 writerX, writerY;
    };

    Vec<Shelf> m_shelves = Vec<Shelf>::create();
    i32 m_atlasWidth, m_atlasHeight;
};

struct AtlasGlyphKey {
    FontFace fontFace;
    u32 pixelSize;
    u32 glyphIndex;

    bool operator==(const AtlasGlyphKey& other) const {
        return fontFace == other.fontFace && pixelSize == other.pixelSize && glyphIndex == other.glyphIndex;
    }
};

struct AtlasGlyphKeyHash {
    static u64 hash(const AtlasGlyphKey& key) noexcept {
        return XXH3_64bits(&key, sizeof(key));
    }
};

struct AtlasGlyphParams {
    math::Vector2f texcoordStart;
    math::Vector2f texcoordEnd;
    u32 imageShaderIndex;
    math::Vector2f bearing;
    math::Vector2f size;
    float advance;
};

struct DynamicBitmapFontAtlas {
    struct AtlasTexture {
        vkrhi::VulkanImage image;
        u32 imageShaderIndex;

        AtlasShelfPacker imagePacker;
    };

    Vec<AtlasTexture> m_atlasTextures = Vec<AtlasTexture>::create();
    HashMap<AtlasGlyphKey, AtlasGlyphParams, AtlasGlyphKeyHash> m_glyphParams = HashMap<AtlasGlyphKey, AtlasGlyphParams, AtlasGlyphKeyHash>::withCapacity(64);

    auto insertGlyphParam(FontFace fontFace, u32 pixelSize, u32 glyphIndex, math::Vector2f texcoordStart, math::Vector2f texcoordEnd, u32 imageShaderIndex, math::Vector2f bearing, math::Vector2f size, float advance) -> void {
        m_glyphParams.insert(AtlasGlyphKey { fontFace, pixelSize, glyphIndex }, AtlasGlyphParams { texcoordStart, texcoordEnd, imageShaderIndex, bearing, size, advance });
    }

    auto hasGlyphParam(FontFace fontFace, u32 pixelSize, u32 glyphIndex) -> bool {
        return m_glyphParams.contains(AtlasGlyphKey { fontFace, pixelSize, glyphIndex });
    }

    auto getGlyphParams(FontFace fontFace, u32 pixelSize, u32 glyphIndex) -> AtlasGlyphParams& {
        return m_glyphParams[AtlasGlyphKey { fontFace, pixelSize, glyphIndex }];
    }

    auto pushNewImage(u32 width, u32 height) -> void {
        auto name = fmt::format("Font bitmap atlas image #{}", m_atlasTextures.size());
        auto image = vkrhi::VulkanImage::builder()
            .name(name)
            .type(vk::ImageType::e2D)
            .extentsrd(vk::Extent3D { width, height, 1 }, 1, 1)
            .isCubemap(false)
            .format(vk::Format::eR8Unorm)
            .tiling(vk::ImageTiling::eOptimal)
            .usage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
            .memoryUsage(vma::MemoryUsage::eAutoPreferDevice)
            .queueFamilyIndices(vkrhi::QueueFamily::Graphics)
            .initialLayout(vk::ImageLayout::eUndefined)
            .build();

        auto imageShaderIndex = TextureManager::get().shaderResourceTable().allocateSampledImageIndex();
        TextureManager::get().shaderResourceTable().bindSampledImage(image, imageShaderIndex);
        m_atlasTextures.emplace(AtlasTexture { std::move(image), imageShaderIndex.imageIndex, AtlasShelfPacker(width, height) });
    }
};

}
