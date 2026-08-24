export module projnekomata:core.ui.nodes.box;
import projnekomata.cs;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

class UIBoxBuilderNeedsChild;
class UIBoxBuilder;
class UIBox : public UINodeSingleChild {
public:
    explicit UIBox(Unique<UINode>&& child) : UINodeSingleChild(std::move(child)) {}
    template <typename D> explicit UIBox(Unique<D>&& child) : UINodeSingleChild(Unique<UINode>::upcast(std::move(child))) {}

    constexpr static auto builder() -> UIBoxBuilderNeedsChild;
    
    Extent2D posOffset = Extent2D(ExtentPx{0.f}, ExtentPx{0.f});
    Extent2D size = Extent2D(ExtentPercent{100.f}, ExtentPercent{100.f});
    math::Vector2f anchor = math::Vector2f(0.0f, 0.0f);

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {
        m_child->scanForUnrasterizedGlyphs(dstFontRasterBatches, fontAtlas);
    }

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f(0.0f);

        auto position = resolveExtent2D(posOffset.x, posOffset.y, bounds.extents());
        auto si = resolveExtent2D(size.x, size.y, bounds.extents());
        auto childBounds = bounds;
        childBounds.min() += position;
        childBounds.max() = childBounds.min() + si;

        auto listLenBefore = ctx.drawCmds.len();
        auto hitregionsLenBefore = ctx.mouseHitRegions.len();
        auto textInputPtrBefore = ctx.textInputCtx.dstText;
        auto childSize = m_child->buildDrawCmds(ctx, inheritanceCtx, childBounds);
        auto offset = (bounds.extents() - childSize).componentWiseMultiply(anchor);

        for (auto i = listLenBefore; i < ctx.drawCmds.len(); i++) {
            auto& cmd = ctx.drawCmds[i];
            match(cmd,
                [&](UiTextDrawCmd& cmd) { cmd.ssPosition += offset; },
                [&](UiRectDrawCmd& cmd) { cmd.ssBegin += offset; cmd.ssEnd += offset; },
                [&](UiTextureDrawCmd& cmd) { cmd.ssBegin += offset; cmd.ssEnd += offset; }
            );
        }

        for (auto i = hitregionsLenBefore; i < ctx.mouseHitRegions.len(); i++) {
            auto& hitregion = ctx.mouseHitRegions[i];
            hitregion.position += offset;
        }

        if (ctx.textInputCtx.dstText != textInputPtrBefore) {
            ctx.textInputCtx.rect.min() += offset;
            ctx.textInputCtx.rect.max() += offset;
        }

        return si;
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        auto si = resolveExtent2D(size.x, size.y, constraints.maxSize);

        auto childconstraints = MeasureConstraints{si};
        m_child->measure(fontAtlas, childconstraints);

        return constraints.maxSize;
    }
};


enum class AnchorPreset {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

constexpr auto resolveAnchorPreset(AnchorPreset preset) -> math::Vector2f {
    switch (preset) {
        case AnchorPreset::TopLeft: return math::Vector2f(0.0f, 0.0f);
        case AnchorPreset::TopCenter: return math::Vector2f(0.5f, 0.0f);
        case AnchorPreset::TopRight: return math::Vector2f(1.0f, 0.0f);
        case AnchorPreset::MiddleLeft: return math::Vector2f(0.0f, 0.5f);
        case AnchorPreset::MiddleCenter: return math::Vector2f(0.5f, 0.5f);
        case AnchorPreset::MiddleRight: return math::Vector2f(1.0f, 0.5f);
        case AnchorPreset::BottomLeft: return math::Vector2f(0.0f, 1.0f);
        case AnchorPreset::BottomCenter: return math::Vector2f(0.5f, 1.0f);
        case AnchorPreset::BottomRight: return math::Vector2f(1.0f, 1.0f);
    }
}

class UIBoxBuilder {
public:
    constexpr auto positionX(float pixels) -> UIBoxBuilder& { m_box->posOffset.x = ExtentPx{pixels}; return *this; }
    constexpr auto positionY(float pixels) -> UIBoxBuilder& { m_box->posOffset.y = ExtentPx{pixels}; return *this; }
    constexpr auto position(math::Vector2f pos) -> UIBoxBuilder& { m_box->posOffset.x = ExtentPx{pos.x()}; m_box->posOffset.y = ExtentPx{pos.y()}; return *this; }
    constexpr auto positionPercX(float percent) -> UIBoxBuilder& { m_box->posOffset.x = ExtentPercent{percent}; return *this; }
    constexpr auto positionPercY(float percent) -> UIBoxBuilder& { m_box->posOffset.y = ExtentPercent{percent}; return *this; }
    constexpr auto positionPercent(math::Vector2f percent) -> UIBoxBuilder& { m_box->posOffset.x = ExtentPercent{percent.x()}; m_box->posOffset.y = ExtentPercent{percent.y()}; return *this; }

    constexpr auto extentX(float pixels) -> UIBoxBuilder& { m_box->size.x = ExtentPx{pixels}; return *this; }
    constexpr auto extentY(float pixels) -> UIBoxBuilder& { m_box->size.y = ExtentPx{pixels}; return *this; }
    constexpr auto extent(math::Vector2f extent) -> UIBoxBuilder& { m_box->size.x = ExtentPx{extent.x()}; m_box->size.y = ExtentPx{extent.y()}; return *this; }
    constexpr auto extentPercentX(float percent) -> UIBoxBuilder& { m_box->size.x = ExtentPercent{percent}; return *this; }
    constexpr auto extentPercentY(float percent) -> UIBoxBuilder& { m_box->size.y = ExtentPercent{percent}; return *this; }
    constexpr auto extentPercent(math::Vector2f percent) -> UIBoxBuilder& { m_box->size.x = ExtentPercent{percent.x()}; m_box->size.y = ExtentPercent{percent.y()}; return *this; }

    constexpr auto anchorX(float percent) -> UIBoxBuilder& { m_box->anchor.x() = percent; return *this; }
    constexpr auto anchorY(float percent) -> UIBoxBuilder& { m_box->anchor.y() = percent; return *this; }
    constexpr auto anchor(math::Vector2f anchor) -> UIBoxBuilder& { m_box->anchor = anchor; return *this; }
    constexpr auto anchorPreset(AnchorPreset preset) -> UIBoxBuilder& { m_box->anchor = resolveAnchorPreset(preset); return *this; }

    constexpr auto visible(bool visible) -> UIBoxBuilder& { m_box->visible = visible; return *this; }

    auto build() -> Unique<UIBox> { return std::move(m_box); }
    
private:
    UIBoxBuilder(Unique<UINode>&& child) : m_box(Unique<UIBox>::create(std::move(child))) {}
    Unique<UIBox> m_box;
    
    friend class UIBoxBuilderNeedsChild;
};

class UIBoxBuilderNeedsChild {
public:
    template <typename D> auto child(Unique<D>&& child) -> UIBoxBuilder {
        return UIBoxBuilder(Unique<UINode>::upcast(std::move(child)));
    }
};
constexpr auto UIBox::builder() -> UIBoxBuilderNeedsChild { return UIBoxBuilderNeedsChild{}; }

}