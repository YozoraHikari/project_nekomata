export module projnekomata:core.ui.nodes.node;
import projnekomata.corelib;
import :core.math;
import :core.overloaded;
import :core.ui.ui_drawcmds;
import :core.ui.element_style;
import :graphics.fontsystem.font_manager;
import :core.input.keys;

export namespace projnekomata::ui {

class UINode;

struct ExtentPx { float pixels; };
struct ExtentPercent { float percent; };
using Extent = FlatVariant<ExtentPx, ExtentPercent>;
struct Extent2D { Extent x; Extent y; };

constexpr auto resolveExtent(Extent e, float extentBounds) -> float {
    return match(e,
        [](const ExtentPx& px) { return px.pixels; },
        [&](const ExtentPercent& percent) { return (percent.percent / 100.f) * extentBounds; }
    );
}

constexpr auto resolveExtent2D(Extent x, Extent y, math::Vector2f extentBounds) -> math::Vector2f {
    return math::Vector2f(resolveExtent(x, extentBounds.x()), resolveExtent(y, extentBounds.y()));
}

enum class InvalidateLayoutReason {
    General,
};

struct UiMouseHitRegion {
    math::Vector2f position;
    math::Vector2f extent;
    UINode* ref;

    bool capturesClicks = false;
    bool capturesHover = false;

    Option<std::function<auto(math::Vector2f) -> void>> clickCallback;
    Option<std::function<auto(math::Vector2f) -> void>> hoverCallback;
};

struct TextInputCtx {
    std::string* dstText = nullptr;
    math::Aabb2f rect;
};

struct BuildCtx {
    Vec<UiDrawCmd>& drawCmds;
    Vec<UiMouseHitRegion>& mouseHitRegions;

    gfx::DynamicBitmapFontAtlas& fontAtlas;

    TextInputCtx& textInputCtx;

    UINode* clickedElement = nullptr;
    UINode* hoveredElement = nullptr;
    UINode* focusedElement = nullptr;
};

struct InheritanceCtx {
    bool parentIsClicked = false;
    bool parentIsHovered = false;
    bool parentIsFocused = false;
};

struct MeasureConstraints {
    math::Vector2f maxSize;
};

class UINode {
public:
    explicit UINode() = default;
    virtual ~UINode() = default;

    virtual auto onKeyDown(core::input::Key key, core::input::KeyModifierFlags mod, bool repeat) -> bool { return false; }

    virtual auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void = 0;

    auto measure(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f {
        if (measuredSizeInvalidated) {
            measuredSize = measureSelf(fontAtlas, constraints);
            measuredSizeInvalidated = false;
        }
        return measuredSize;
    }

    virtual auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f = 0;

    // ---- Layout ---------------------------------------------------------------------------------------------------------------------------------------------

    UINode* parent = nullptr;

    // ---- Metadata -------------------------------------------------------------------------------------------------------------------------------------------

    bool visible = true;

    math::Vector2f measuredSize;
    bool measuredSizeInvalidated = true;

    auto invalidateLayout(InvalidateLayoutReason reason) -> void {
        measuredSizeInvalidated = true;
        if (parent != nullptr) parent->invalidateLayout(reason);
    }

protected:
    virtual auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f = 0;
};

class UINodeSingleChild : public UINode {
public:
    explicit UINodeSingleChild(Unique<UINode>&& child) : m_child(std::move(child)) {
        m_child->parent = this;
    }
    template <typename D> explicit UINodeSingleChild(Unique<D>&& child) : UINodeSingleChild(Unique<UINode>::upcast(std::move(child))) {
        m_child->parent = this;
    }

protected:
    Unique<UINode> m_child;
};

class UINodeMultiChild : public UINode {
public:
    explicit UINodeMultiChild() = default;
    explicit UINodeMultiChild(Vec<Unique<UINode>>&& children) : m_children(std::move(children)) {}

    auto addChild(Unique<UINode>&& child) -> void {
        child->parent = this;
        m_children.emplace(std::move(child));
    }

    template <typename D> auto addChild(Unique<D>&& child) -> void {
        addChild(Unique<UINode>::upcast(std::move(child)));
    }

protected:
    Vec<Unique<UINode>> m_children;
};

}
