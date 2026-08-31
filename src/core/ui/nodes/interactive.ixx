export module projnekomata:core.ui.nodes.interactive;
import projnekomata.corelib;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

class UIInteractiveBuilder;
class UIInteractiveBuilderNeedsChild;
class UIInteractive : public UINodeSingleChild {
public:
    explicit UIInteractive(Unique<UINode>&& child) : UINodeSingleChild(std::move(child)) {}
    explicit UIInteractive(Unique<UINode>&& child, std::function<auto(math::Vector2f) -> void>&& onClick, std::function<auto(math::Vector2f) -> void>&& onHover)
        : UINodeSingleChild(std::move(child)), clickCallback(std::move(onClick)), hoverCallback(std::move(onHover)) {}

    template <typename D> explicit UIInteractive(Unique<D>&& child) : UINodeSingleChild(Unique<UINode>::upcast(std::move(child))) {}
    template <typename D> explicit UIInteractive(Unique<D>&& child, std::function<auto(math::Vector2f) -> void>&& onClick, std::function<auto(math::Vector2f) -> void>&& onHover)
        : UINodeSingleChild(Unique<UINode>::upcast(std::move(child))), clickCallback(std::move(onClick)), hoverCallback(std::move(onHover)) {}

    constexpr static auto builder() -> UIInteractiveBuilderNeedsChild;

    std::function<auto(math::Vector2f) -> void> clickCallback = nullptr;
    std::function<auto(math::Vector2f) -> void> hoverCallback = nullptr;
    bool capturesClicks = true;
    bool capturesHover = true;

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {
        m_child->scanForUnrasterizedGlyphs(dstFontRasterBatches, fontAtlas);
    }

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f::zero();

        inheritanceCtx.parentIsClicked |= (ctx.clickedElement == this);
        inheritanceCtx.parentIsHovered |= (ctx.hoveredElement == this);

        if (capturesClicks || capturesHover) {
            auto optClickCallback = Option<std::function<auto(math::Vector2f) -> void>>::someIf(
                capturesClicks && (clickCallback != nullptr),
                [&] { return std::function<auto(math::Vector2f) -> void>(clickCallback); }
            );
            auto optHoverCallback = Option<std::function<auto(math::Vector2f) -> void>>::someIf(
                capturesHover && (hoverCallback != nullptr),
                [&] { return std::function<auto(math::Vector2f) -> void>(hoverCallback); }
            );

            ctx.mouseHitRegions.emplace(UiMouseHitRegion{
                .position = bounds.min(),
                .extent = bounds.extents(),
                .ref = static_cast<UINode*>(this),
                .capturesClicks = capturesClicks,
                .capturesHover = capturesHover,
                .clickCallback = optClickCallback,
                .hoverCallback = optHoverCallback
            });
        }

        return m_child->buildDrawCmds(ctx, inheritanceCtx, bounds);
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        return m_child->measure(fontAtlas, constraints);
    }
};

class UIInteractiveBuilder {
public:
    auto capturesClicks(bool capturesClicks) -> UIInteractiveBuilder& { m_interactive->capturesClicks = capturesClicks; return *this; }
    auto capturesHover(bool capturesHover) -> UIInteractiveBuilder& { m_interactive->capturesHover = capturesHover; return *this; }
    auto onClick(std::function<auto(math::Vector2f) -> void>&& onClick) -> UIInteractiveBuilder& { m_interactive->clickCallback = std::move(onClick); return *this; }
    auto onHover(std::function<auto(math::Vector2f) -> void>&& onHover) -> UIInteractiveBuilder& { m_interactive->hoverCallback = std::move(onHover); return *this; }

    constexpr auto visible(bool visible) -> UIInteractiveBuilder& { m_interactive->visible = visible; return *this; }

    auto build() -> Unique<UIInteractive> { return std::move(m_interactive); }

private:
    UIInteractiveBuilder(Unique<UINode>&& child) : m_interactive(Unique<UIInteractive>::create(std::move(child))) {}
    Unique<UIInteractive> m_interactive;

    friend class UIInteractiveBuilderNeedsChild;
};

class UIInteractiveBuilderNeedsChild {
public:
    template <typename D> auto child(Unique<D>&& child) -> UIInteractiveBuilder {
        return UIInteractiveBuilder(Unique<UINode>::upcast(std::move(child)));
    }
};
constexpr auto UIInteractive::builder() -> UIInteractiveBuilderNeedsChild { return UIInteractiveBuilderNeedsChild{}; }

}