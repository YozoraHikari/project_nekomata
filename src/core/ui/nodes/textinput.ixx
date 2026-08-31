export module projnekomata:core.ui.nodes.textinput;
import projnekomata.corelib;
import std;
import :core.ui.nodes.node;
import :core.ui.ui_system;
import :core.command.shell;

export namespace projnekomata::ui {

class UITextInput : public UINode {
public:
    explicit UITextInput(std::string initialText, Option<std::string> hintText, float textSize, FontFace fontFace, std::function<void(std::string)> onSubmit)
        : currentText(std::move(initialText)), hintText(std::move(hintText)), textSize(textSize), fontFace(std::move(fontFace)), onSubmit(std::move(onSubmit)) {}

    ~UITextInput() override {
        UiSystem::get().releaseFocusIfMatches(this);
    }

    constexpr static auto create(std::string initialText, Option<std::string> hintText, float size, FontFace fontFace, std::function<void(std::string)> onSubmit) -> Unique<UITextInput> { return Unique<UITextInput>::create(std::move(initialText), std::move(hintText), size, std::move(fontFace), std::move(onSubmit)); }

    ElementStyle style = ElementStyle();
    ElementStyle hintStyle = ElementStyle::builder().color(Color::fromRgb32Float(0.5f, 0.5f, 0.5f)).build();

    std::string currentText;
    Option<std::string> hintText;
    float textSize;
    FontFace fontFace;
    std::function<void(std::string)> onSubmit;

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {
        auto batch = FontManager::get().findAndBatchMissingGlyphs(fontFace, fontAtlas, currentText, textSize);
        if (batch.isSome()) dstFontRasterBatches.emplace(std::move(batch.unwrap()));

        if (hintText.isSome()) {
            auto batch = FontManager::get().findAndBatchMissingGlyphs(fontFace, fontAtlas, hintText.unwrap(), textSize);
            if (batch.isSome()) dstFontRasterBatches.emplace(std::move(batch.unwrap()));
        }
    }

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) {
            UiSystem::get().releaseFocusIfMatches(this);
            return math::Vector2f::zero();
        }

        if (ctx.focusedElement == this) {
            ctx.textInputCtx.dstText = &currentText;
            ctx.textInputCtx.rect = bounds;
        }

        ctx.mouseHitRegions.emplace(UiMouseHitRegion{
            .position = bounds.min(),
            .extent = bounds.extents(),
            .ref = static_cast<UINode*>(this),
            .capturesClicks = true,
            .capturesHover = true,
            .clickCallback = None,
            .hoverCallback = None
        });

        if (hintText.isSome() && currentText.empty() && ctx.focusedElement != this) {
            auto glyphs = FontManager::get().shapeText(fontFace, ctx.fontAtlas, hintText.unwrap(), textSize, false).first;
            ctx.drawCmds.emplace(UiTextDrawCmd{
                .ssPosition = bounds.min(),
                .glyphs = std::move(glyphs),
                .color = hintStyle.getEndColor(inheritanceCtx.parentIsHovered, inheritanceCtx.parentIsClicked)
            });

            return bounds.extents();
        }

        auto [glyphs, feedback] = FontManager::get().shapeText(fontFace, ctx.fontAtlas, currentText, textSize, true);
        m_glyphFeedback = std::move(feedback);

        ctx.drawCmds.emplace(UiTextDrawCmd{
            .ssPosition = bounds.min(),
            .glyphs = std::move(glyphs),
            .color = style.getEndColor(inheritanceCtx.parentIsHovered, inheritanceCtx.parentIsClicked)
        });

        return bounds.extents();
    }

    auto onKeyDown(core::input::Key key, core::input::KeyModifierFlags mod, bool repeat) -> bool override {
        if (key == core::input::Key::Enter) {
            auto submittedText = currentText;
            currentText = "";

            onSubmit(std::move(submittedText));

            return true;
        }

        return false;
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        if (currentText.empty() && hintText.isSome()) {
            return FontManager::get().measureText(fontFace, fontAtlas, hintText.unwrap(), textSize);
        }
        return FontManager::get().measureText(fontFace, fontAtlas, currentText, textSize);
    }

    Vec<GlyphFeedback> m_glyphFeedback;
};

}
