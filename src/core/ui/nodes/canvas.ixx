export module projnekomata:core.ui.nodes.canvas;
import projnekomata.corelib;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

class UICanvasBuilder;
class UICanvas : public UINodeMultiChild {
public:
    explicit UICanvas() = default;
    explicit UICanvas(Vec<Unique<UINode>>&& children) : UINodeMultiChild(std::move(children)) {}

    constexpr static auto create() -> Unique<UICanvas> { return Unique<UICanvas>::create(); }
    constexpr static auto builder() -> UICanvasBuilder;

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {
        for (auto& child : m_children) child->scanForUnrasterizedGlyphs(dstFontRasterBatches, fontAtlas);
    }

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f::zero();

        for (auto& child : m_children) child->buildDrawCmds(ctx, inheritanceCtx, bounds);
        return bounds.extents();
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        for (auto& child : m_children) child->measure(fontAtlas, constraints);
        return constraints.maxSize;
    }

private:
    friend class UICanvasBuilder;
};

class UICanvasBuilder {
public:
    auto children(Vec<Unique<UINode>>&& children) -> UICanvasBuilder& { m_canvas->m_children = std::move(children); return *this; }
    auto addChild(Unique<UINode>&& child) -> UICanvasBuilder& { m_canvas->addChild(std::move(child)); return *this; }
    template <typename D> auto addChild(Unique<D>&& child) -> UICanvasBuilder& { m_canvas->addChild(Unique<UINode>::upcast(std::move(child))); return *this; }

    constexpr auto visible(bool visible) -> UICanvasBuilder& { m_canvas->visible = visible; return *this; }

    auto build() -> Unique<UICanvas> { return std::move(m_canvas); }

private:
    Unique<UICanvas> m_canvas = Unique<UICanvas>::create();
};
constexpr auto UICanvas::builder() -> UICanvasBuilder { return UICanvasBuilder{}; }

}