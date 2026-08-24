export module projnekomata:core.ui.nodes.rect;
import projnekomata.cs;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

class UIRectBuilder;
class UIRect : public UINode {
public:
    explicit UIRect() {}

    constexpr static auto builder() -> UIRectBuilder;

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {}

    ElementStyle style = ElementStyle();

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f(0.0f);

        ctx.drawCmds.emplace(UiRectDrawCmd{
            .ssBegin  = bounds.min(),
            .ssEnd    = bounds.max(),
            .color    = style.getEndColor(false, false)
        });
        return bounds.extents();
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        return constraints.maxSize;
    }
};

class UIRectBuilder {
public:
    auto style(ElementStyle style) -> UIRectBuilder& { m_rect->style = style; return *this; }

    constexpr auto visible(bool visible) -> UIRectBuilder& { m_rect->visible = visible; return *this; }

    auto build() -> Unique<UIRect> { return std::move(m_rect); }

private:
    Unique<UIRect> m_rect = Unique<UIRect>::create();
};
constexpr auto UIRect::builder() -> UIRectBuilder { return UIRectBuilder(); }

}