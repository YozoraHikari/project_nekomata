module;
#include <SDL3/SDL.h>
module projnekomata;
import :core.ui.ui_system;

namespace projnekomata::ui {

UiSystem::UiSystem(std::nullptr_t) {  }

auto UiSystem::create() -> Unique<UiSystem> {
    debug_assert(g_uiSystem == nullptr, "UiSystem already exists");
    auto inst = Unique<UiSystem>::create(nullptr);
    g_uiSystem = inst.ptr();

    inst->m_uiRoot = UICanvas::builder()
        .build();

    return inst;
}

auto UiSystem::scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void {
    m_uiRoot->scanForUnrasterizedGlyphs(dstFontRasterBatches, fontAtlas);
}

auto UiSystem::buildUi(Vec<ui::UiDrawCmd>& drawcmds, gfx::DynamicBitmapFontAtlas& fontAtlas, math::Vector2f screenLogicalSize, SdlWindow& window) -> void {
    m_lastFrameMouseHitRegions.clear();
    m_textInputCtx.dstText = nullptr;

    // Run the measurement pass:
    m_uiRoot->measure(fontAtlas, MeasureConstraints{screenLogicalSize});

    // Build the UI draw commands:
    auto buildCtx = BuildCtx {
        .drawCmds = drawcmds,
        .mouseHitRegions = m_lastFrameMouseHitRegions,
        .fontAtlas = fontAtlas,
        .textInputCtx = m_textInputCtx,
        .clickedElement = m_pressedElement,
        .hoveredElement = m_hoveredElement,
        .focusedElement = m_focusedElement
    };

    auto inheritanceCtx = InheritanceCtx {
        .parentIsClicked = false,
        .parentIsHovered = false,
        .parentIsFocused = false
    };

    m_uiRoot->buildDrawCmds(buildCtx, inheritanceCtx, Aabb2f(Vector2f(0.0f), screenLogicalSize));

    if (!m_textInputActive && m_textInputCtx.dstText != nullptr) {
        log::info("Text Input is now active");
        m_textInputActive = true;
        SDL_StartTextInput(window.handle());
        auto rect = SDL_Rect{
            .x = static_cast<int>(m_textInputCtx.rect.min().x()),
            .y = static_cast<int>(m_textInputCtx.rect.min().y()),
            .w = static_cast<int>(m_textInputCtx.rect.extents().x()),
            .h = static_cast<int>(m_textInputCtx.rect.extents().y())
        };
        SDL_SetTextInputArea(window.handle(), &rect, 0);
    }

    if (m_textInputActive && m_textInputCtx.dstText == nullptr) {
        log::info("Text Input is now inactive");
        m_textInputActive = false;
        SDL_StopTextInput(window.handle());
    }
}

auto UiSystem::testMouseDownHit(math::Vector2f pos) -> void {
    m_focusedElement = nullptr;
    for (auto& [position, extent, ref, capturesClicks, _, _, _] : m_lastFrameMouseHitRegions.iterRev()) {
        // todo: Make a math box/aabb type to do this
        if (position.x() <= pos.x() && pos.x() <= position.x() + extent.x()
            && position.y() <= pos.y() && pos.y() <= position.y() + extent.y()
            && capturesClicks)
        {
            log::info("Focused Element is now 0x{:016x} <<{}>>", reinterpret_cast<usize>(ref), typeid(*ref).name());
            m_pressedElement = ref;
            m_focusedElement = ref;
        }
    }
}
auto UiSystem::testMouseUpHit(math::Vector2f pos) -> void {
    for (auto& [position, extent, ref, capturesClicks, _, clickCallback, _] : m_lastFrameMouseHitRegions.iterRev()) {
        if (
            ref == m_pressedElement
             && position.x() <= pos.x() && pos.x() <= position.x() + extent.x()
             && position.y() <= pos.y() && pos.y() <= position.y() + extent.y()
             && capturesClicks
             && clickCallback.isSome()
        ) {
            auto& callback = clickCallback.unwrap();
            callback(pos);
        }
    }
    m_pressedElement = nullptr;
}
auto UiSystem::testMouseHover(math::Vector2f pos) -> void {
    m_hoveredElement = nullptr;
    for (auto& [position, extent, ref, _, capturesHover, _, hoverCallback] : m_lastFrameMouseHitRegions.iterRev()) {
        if (
            position.x() <= pos.x() && pos.x() <= position.x() + extent.x()
            && position.y() <= pos.y() && pos.y() <= position.y() + extent.y()
            && capturesHover
        ) {
            m_hoveredElement = ref;

            if (hoverCallback.isSome()) {
                auto& callback = hoverCallback.unwrap();
                callback(pos);
            }
        }
    }
}

auto UiSystem::releaseFocusIfMatches(ui::UINode* element) -> void {
    if (m_focusedElement == element) {
        m_focusedElement = nullptr;
    }
}

auto UiSystem::processKeydown(core::input::Key key, core::input::KeyModifierFlags mod) -> void {
    UINode* focusedElement = m_focusedElement;
    while (focusedElement != nullptr) {
        if (focusedElement->onKeyDown(key, mod, false)) {
            return;
        }
        focusedElement = focusedElement->parent;
    }


    if (wantsTextInputFocus()) {
        if (key == core::input::Key::Backspace) {
            auto& dstText = *m_textInputCtx.dstText;

            if (dstText.empty()) return;

            auto i = dstText.size() - 1;
            while (i > 0 && (dstText[i] & 0xc0) == 0x80) i--;

            dstText.erase(i);
        }

        if (mod == core::input::KeyModifierFlags::LControl && key == core::input::Key::V) {
            auto text = SDL_GetClipboardText();
            log::info("Ctrl+V pressed, text: {}", text);
            if (text != nullptr) {
                m_textInputCtx.dstText->append(text);
                SDL_free(text);
            }
        }
    }
}

auto UiSystem::textInputProcessInput(const char* sdlInput) -> void {
    if (!wantsTextInputFocus()) return;
    m_textInputCtx.dstText->append(sdlInput);
}
} // namespace projnekomata::ui