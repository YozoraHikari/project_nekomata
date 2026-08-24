export module projnekomata:core.ui.nodes.stack;
import projnekomata.corelib;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

enum class StackDirection {
    TopToBottom,
    LeftToRight,
};

class UIStackBuilder;
class UIStack : public UINodeMultiChild {
public:
    explicit UIStack() = default;
    explicit UIStack(Vec<Unique<UINode>>&& children) : UINodeMultiChild(std::move(children)) {}

    constexpr static auto builder() -> UIStackBuilder;

    StackDirection direction = StackDirection::TopToBottom;
    float spacing = 0.0f;

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {
        for (auto& child : m_children) child->scanForUnrasterizedGlyphs(dstFontRasterBatches, fontAtlas);
    }

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f(0.0f);

        auto childBounds = bounds;
        for (auto& child : m_children) {
            auto childSize = child->buildDrawCmds(ctx, inheritanceCtx, childBounds);
            switch (direction) {
                case StackDirection::TopToBottom:
                    childBounds.min().y() += childSize.y() + spacing;
                    break;
                case StackDirection::LeftToRight:
                    childBounds.min().x() += childSize.x() + spacing;
                    break;
            }
        }
        return bounds.extents();
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        for (auto& child : m_children) child->measure(fontAtlas, constraints);
        return constraints.maxSize;
    }

private:
    friend class UIStackBuilder;
};

class UIStackBuilder {
public:
    auto direction(StackDirection direction) -> UIStackBuilder& { m_stack->direction = direction; return *this; }
    auto spacing(float spacing) -> UIStackBuilder& { m_stack->spacing = spacing; return *this; }
    auto children(Vec<Unique<UINode>>&& children) -> UIStackBuilder& { m_stack->m_children = std::move(children); return *this; }
    auto addChild(Unique<UINode>&& child) -> UIStackBuilder& { m_stack->addChild(std::move(child)); return *this; }
    template <typename D> auto addChild(Unique<D>&& child) -> UIStackBuilder& { m_stack->addChild(Unique<UINode>::upcast(std::move(child))); return *this; }

    constexpr auto visible(bool visible) -> UIStackBuilder& { m_stack->visible = visible; return *this; }

    auto build() -> Unique<UIStack> { return std::move(m_stack); }

private:
    UIStackBuilder() : m_stack(Unique<UIStack>::create()) {}
    Unique<UIStack> m_stack;

    friend class UIStack;
};
constexpr auto UIStack::builder() -> UIStackBuilder { return UIStackBuilder(); }

}