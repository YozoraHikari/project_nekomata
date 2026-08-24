export module projnekomata:core.ui.nodes.text;
import projnekomata.cs;
import std;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

class UITextBuilder;
class UIText : public UINode {
public:
    explicit UIText(std::string text, float size, FontFace fontFace) : fontFace(std::move(fontFace)), text(std::move(text)), size(size) {}

    constexpr static auto create(std::string text, float size, FontFace fontFace) -> Unique<UIText> { return Unique<UIText>::create(std::move(text), size, std::move(fontFace)); }
    constexpr static auto builder(std::string text, float size, FontFace fontFace) -> UITextBuilder;

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {
        auto batch = FontManager::get().findAndBatchMissingGlyphs(fontFace, fontAtlas, text, size);
        if (batch.isSome()) dstFontRasterBatches.emplace(std::move(batch.unwrap()));
    }

    ElementStyle style = ElementStyle();
    FontFace fontFace;
    std::string text;
    float size;

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f(0.0f);

        auto glyphs = FontManager::get().shapeText(fontFace, ctx.fontAtlas, text, size, false).first;
        ctx.drawCmds.emplace(UiTextDrawCmd{
            .ssPosition = bounds.min(),
            .glyphs = std::move(glyphs),
            .color = style.getEndColor(inheritanceCtx.parentIsHovered, inheritanceCtx.parentIsClicked)
        });

        return FontManager::get().measureText(fontFace, ctx.fontAtlas, text, size);
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        return FontManager::get().measureText(fontFace, fontAtlas, text, size);
    }
};

class UITextBuilder {
public:
    auto fontFace(FontFace fontFace) -> UITextBuilder& { m_text->fontFace = fontFace; return *this; }
    auto text(std::string text) -> UITextBuilder& { m_text->text = std::move(text); return *this; }
    auto size(float size) -> UITextBuilder& { m_text->size = size; return *this; }
    auto style(ElementStyle style) -> UITextBuilder& { m_text->style = style; return *this; }

    constexpr auto visible(bool visible) -> UITextBuilder& { m_text->visible = visible; return *this; }

    auto build() -> Unique<UIText> { return std::move(m_text); }

private:
    UITextBuilder(std::string text, float size,FontFace fontFace) : m_text(Unique<UIText>::create(std::move(text), size, std::move(fontFace))) {}
    Unique<UIText> m_text;

    friend class UIText;
};
constexpr auto UIText::builder(std::string text, float size, FontFace fontFace) -> UITextBuilder { return UITextBuilder(std::move(text), size, std::move(fontFace)); }

}