export module projnekomata:core.ui.ui_system;
import std;
import :core.ui.ui_drawcmds;
import :core.ui.nodes.canvas;
import :core.input.keys;

export namespace projnekomata::ui {

inline class UiSystem* g_uiSystem = nullptr;

class UiSystem {
public:
    UiSystem(std::nullptr_t);

    static auto get() -> UiSystem& { return *g_uiSystem; }
    static auto create() -> Unique<UiSystem>;

    auto getRoot() const -> ui::UICanvas& { return *m_uiRoot; }

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void;
    auto buildUi(Vec<UiDrawCmd>& drawcmds, gfx::DynamicBitmapFontAtlas& fontAtlas, math::Vector2f screenLogicalSize, SdlWindow& window) -> void;

    auto testMouseDownHit(math::Vector2f pos) -> void;
    auto testMouseUpHit(math::Vector2f pos) -> void;

    auto testMouseHover(math::Vector2f pos) -> void;

    auto releaseFocusIfMatches(ui::UINode* element) -> void;

    auto wantsTextInputFocus() const -> bool { return m_textInputCtx.dstText != nullptr; }

    auto processKeydown(core::input::Key key, core::input::KeyModifierFlags mod) -> void;
    auto textInputProcessInput(const char* sdlInput) -> void;


private:
    Vec<ui::UiMouseHitRegion>   m_lastFrameMouseHitRegions = Vec<ui::UiMouseHitRegion>::create();
    TextInputCtx                m_textInputCtx = {};
    ui::UINode*                 m_pressedElement = nullptr;
    ui::UINode*                 m_hoveredElement = nullptr;
    ui::UINode*                 m_focusedElement = nullptr;

    Unique<ui::UICanvas> m_uiRoot = nullptr;

    bool m_textInputActive = false;
};


}
