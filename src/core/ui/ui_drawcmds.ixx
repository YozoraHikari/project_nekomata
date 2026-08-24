export module projnekomata:core.ui.ui_drawcmds;
import std;
import :core.math;
import :graphics.texturesystem.texture_manager;
import :graphics.fontsystem.font_face;
import :core.color;
import :graphics.vulkan.vk_buffer;
import :graphics.fontsystem.font_manager;

export namespace projnekomata::ui {

struct UiRectDrawCmd {
    math::Vector2f ssBegin;
    math::Vector2f ssEnd;
    Color color;
};

struct UiTextureDrawCmd {
    math::Vector2f ssBegin;
    math::Vector2f ssEnd;
    math::Vector2f texcoordBegin;
    math::Vector2f texcoordEnd;
    Texture texture;
};

struct UiTextDrawCmd {
    math::Vector2f ssPosition;
    Vec<GlyphInstance> glyphs;
    Color color;
};

using UiDrawCmd = FlatVariant<UiRectDrawCmd, UiTextureDrawCmd, UiTextDrawCmd>;

}
