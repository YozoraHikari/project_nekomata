module;
#include <freetype/freetype.h>
export module projnekomata:graphics.fontsystem.font_manager;
import std;
import vulkan;
import projnekomata.cs;
import :core.math;
import :graphics.fontsystem.font_face;
import :graphics.fontsystem.dynamic_font_atlas;

export namespace projnekomata {
class FontManager;

inline FontManager* g_fontManager = nullptr;

struct FontEntry {
    FT_Face face  = nullptr;

    bool isLoaded = false;

    mutable std::mutex rasterMutex;

    float unitsPerEm = 0.0f;
    float ascender = 0.0f;
    float descender = 0.0f;
};

struct FontRasterBatch {
    FontFace fontFace;
    u32 pixelSize;
    Vec<u32> glyphIndices = Vec<u32>::create();
};

struct FontRasterInfo {
    Slice<const FontRasterBatch> batches;
    gfx::DynamicBitmapFontAtlas& atlas;
    // the u32 is the index in the font atlas image array
    HashMap<u32, Vec<vk::BufferImageCopy2>>& copyRegions;
    Vec<u8>& resultBuffer;
    Vec<u32>& newImageIndices;
};

struct GlyphInstance {
    math::Vector2f ssStart;
    math::Vector2f ssEnd;
    math::Vector2f texcoordStart;
    math::Vector2f texcoordEnd;
    u32 imageShaderIndex;
};

struct GlyphFeedback {
    math::Vector2f position;
};

class FontManager {
public:
    static auto get() -> FontManager& {
        return *g_fontManager;
    }

    FontManager(std::nullptr_t);
    FontManager();
    ~FontManager();

    static auto create() -> Unique<FontManager>;

    auto loadFont(const fs::Path& path) -> FontFace;
    auto freeFont(FontFace font) -> void;

    auto rasterizeGlyphs(FontRasterInfo rasterInfo) -> void;
    auto measureText(FontFace font, gfx::DynamicBitmapFontAtlas& atlas, std::string_view text, u32 pixelSize) -> math::Vector2f;
    auto shapeText(FontFace font, gfx::DynamicBitmapFontAtlas& atlas, std::string_view text, u32 pixelSize, bool feedback) -> std::pair<Vec<GlyphInstance>, Vec<GlyphFeedback>>;
    auto findAndBatchMissingGlyphs(FontFace font, gfx::DynamicBitmapFontAtlas& atlas, std::string_view text, u32 pixelSize) -> Option<FontRasterBatch>;

private:
    FT_Library m_ftLibrary;
    std::mutex m_ftLibraryMutex;

    Vec<Unique<FontEntry>> m_fontEntries = Vec<Unique<FontEntry>>::create();
    std::shared_mutex m_registryMutex;

    u32 getFreeFontIndex();
};

}