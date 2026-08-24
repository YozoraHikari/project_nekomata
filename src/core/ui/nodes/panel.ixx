export module projnekomata:core.ui.nodes.panel;
import projnekomata.corelib;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

class UIPanelBuilder;
class UIPanelBuilderNeedsChild;
class UIPanel : public UINodeSingleChild {
public:
    explicit UIPanel(Unique<UINode>&& child) : UINodeSingleChild(std::move(child)) {}
    template <typename D> explicit UIPanel(Unique<D>&& child) : UINodeSingleChild(Unique<UINode>::upcast(std::move(child))) {}

    constexpr static auto create(Unique<UINode>&& child) -> Unique<UIPanel> { return Unique<UIPanel>::create(std::move(child)); }
    constexpr static auto builder() -> UIPanelBuilderNeedsChild;

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {
        m_child->scanForUnrasterizedGlyphs(dstFontRasterBatches, fontAtlas);
    }

    ElementStyle style = ElementStyle();

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f(0.0f);

        ctx.drawCmds.emplace(UiRectDrawCmd{
            .ssBegin  = bounds.min(),
            .ssEnd    = bounds.max(),
            .color    = style.getEndColor(inheritanceCtx.parentIsHovered, inheritanceCtx.parentIsClicked)
        });

        m_child->buildDrawCmds(ctx, inheritanceCtx, bounds);

        return bounds.extents();
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        m_child->measure(fontAtlas, constraints);

        return constraints.maxSize;
    }
};

class UIPanelBuilder {
public:
    auto style(ElementStyle style) -> UIPanelBuilder& { m_panel->style = style; return *this; }

    constexpr auto visible(bool visible) -> UIPanelBuilder& { m_panel->visible = visible; return *this; }

    auto build() -> Unique<UIPanel> { return std::move(m_panel); }

private:
    UIPanelBuilder(Unique<UINode>&& child) : m_panel(Unique<UIPanel>::create(std::move(child))) {}
    Unique<UIPanel> m_panel;

    friend class UIPanelBuilderNeedsChild;
};

class UIPanelBuilderNeedsChild {
public:
    template <typename D> auto child(Unique<D>&& child) -> UIPanelBuilder {
        return UIPanelBuilder(Unique<UINode>::upcast(std::move(child)));
    }
};
constexpr auto UIPanel::builder() -> UIPanelBuilderNeedsChild { return UIPanelBuilderNeedsChild{}; }

}