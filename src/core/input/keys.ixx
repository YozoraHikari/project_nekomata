module;
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
export module projnekomata:core.input.keys;
import projnekomata.corelib;

export namespace projnekomata::core::input {

enum class Key {
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    LShift, RShift, LControl, RControl, LAlt, RAlt, LSystem, RSystem, Menu,
    Backspace, Tab, Enter, Space, Escape,
    Backtick, Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Apostrophe, Comma, Period, Slash,
    ArrowLeft, ArrowRight, ArrowUp, ArrowDown,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    NavInsert, NavDelete, NavHome, NavEnd, NavPgUp, NavPgDown,
    PrintScreen, ScrollLock, Pause,
    KpNumLock, KpDivide, KpMultiply, KpSubtract, KpAdd, KpEnter,
    Kp0, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
    KpDecimal,
    MouseLeft, MouseRight, MouseMiddle, Mouse4, Mouse5,
    // also used as unknown keycode
    CountDiscrim,
};
constexpr auto kKeyDiscrimsCount = static_cast<u32>(Key::CountDiscrim);

enum class KeyModifierFlags : u32 {
    None = 0x0,
    LShift = 0x1,
    RShift = 0x2,
    AnyShift = LShift | RShift,
    LControl = 0x4,
    RControl = 0x8,
    AnyControl = LControl | RControl,
    LAlt = 0x10,
    RAlt = 0x20,
    AnyAlt = LAlt | RAlt,
    LSystem = 0x40,
    RSystem = 0x80,
    AnySystem = LSystem | RSystem,
    CapsLock = 0x100,
};
constexpr auto operator|(KeyModifierFlags lhs, KeyModifierFlags rhs) -> KeyModifierFlags { return static_cast<KeyModifierFlags>(static_cast<u32>(lhs) | static_cast<u32>(rhs)); }
constexpr auto operator|=(KeyModifierFlags& lhs, KeyModifierFlags rhs) -> KeyModifierFlags& { lhs = static_cast<KeyModifierFlags>(static_cast<u32>(lhs) | static_cast<u32>(rhs)); return lhs; }
constexpr auto operator&(KeyModifierFlags lhs, KeyModifierFlags rhs) -> KeyModifierFlags { return static_cast<KeyModifierFlags>(static_cast<u32>(lhs) & static_cast<u32>(rhs)); }
constexpr auto operator&=(KeyModifierFlags& lhs, KeyModifierFlags rhs) -> KeyModifierFlags& { lhs = static_cast<KeyModifierFlags>(static_cast<u32>(lhs) & static_cast<u32>(rhs)); return lhs; }
constexpr auto operator~(KeyModifierFlags x) -> KeyModifierFlags { return static_cast<KeyModifierFlags>(~static_cast<u32>(x)); }

constexpr auto mapSdlKeyToKey(SDL_Keycode keycode) -> Key {
    switch (keycode) {
    case SDLK_0: return Key::Digit0;
    case SDLK_1: return Key::Digit1;
    case SDLK_2: return Key::Digit2;
    case SDLK_3: return Key::Digit3;
    case SDLK_4: return Key::Digit4;
    case SDLK_5: return Key::Digit5;
    case SDLK_6: return Key::Digit6;
    case SDLK_7: return Key::Digit7;
    case SDLK_8: return Key::Digit8;
    case SDLK_9: return Key::Digit9;
    case SDLK_A: return Key::A;
    case SDLK_B: return Key::B;
    case SDLK_C: return Key::C;
    case SDLK_D: return Key::D;
    case SDLK_E: return Key::E;
    case SDLK_F: return Key::F;
    case SDLK_G: return Key::G;
    case SDLK_H: return Key::H;
    case SDLK_I: return Key::I;
    case SDLK_J: return Key::J;
    case SDLK_K: return Key::K;
    case SDLK_L: return Key::L;
    case SDLK_M: return Key::M;
    case SDLK_N: return Key::N;
    case SDLK_O: return Key::O;
    case SDLK_P: return Key::P;
    case SDLK_Q: return Key::Q;
    case SDLK_R: return Key::R;
    case SDLK_S: return Key::S;
    case SDLK_T: return Key::T;
    case SDLK_U: return Key::U;
    case SDLK_V: return Key::V;
    case SDLK_W: return Key::W;
    case SDLK_X: return Key::X;
    case SDLK_Y: return Key::Y;
    case SDLK_Z: return Key::Z;
    case SDLK_LSHIFT: return Key::LShift;
    case SDLK_RSHIFT: return Key::RShift;
    case SDLK_LCTRL: return Key::LControl;
    case SDLK_RCTRL: return Key::RControl;
    case SDLK_LALT: return Key::LAlt;
    case SDLK_RALT: return Key::RAlt;
    case SDLK_LGUI: return Key::LSystem;
    case SDLK_RGUI: return Key::RSystem;
    case SDLK_MENU: return Key::Menu;
    case SDLK_BACKSPACE: return Key::Backspace;
    case SDLK_TAB: return Key::Tab;
    case SDLK_RETURN: return Key::Enter;
    case SDLK_SPACE: return Key::Space;
    case SDLK_ESCAPE: return Key::Escape;
    case SDLK_GRAVE: return Key::Backtick;
    case SDLK_MINUS: return Key::Minus;
    case SDLK_EQUALS: return Key::Equals;
    case SDLK_LEFTBRACKET: return Key::LeftBracket;
    case SDLK_RIGHTBRACKET: return Key::RightBracket;
    case SDLK_BACKSLASH: return Key::Backslash;
    case SDLK_SEMICOLON: return Key::Semicolon;
    case SDLK_APOSTROPHE: return Key::Apostrophe;
    case SDLK_COMMA: return Key::Comma;
    case SDLK_PERIOD: return Key::Period;
    case SDLK_SLASH: return Key::Slash;
    case SDLK_UP: return Key::ArrowUp;
    case SDLK_DOWN: return Key::ArrowDown;
    case SDLK_LEFT: return Key::ArrowLeft;
    case SDLK_RIGHT: return Key::ArrowRight;
    case SDLK_F1: return Key::F1;
    case SDLK_F2: return Key::F2;
    case SDLK_F3: return Key::F3;
    case SDLK_F4: return Key::F4;
    case SDLK_F5: return Key::F5;
    case SDLK_F6: return Key::F6;
    case SDLK_F7: return Key::F7;
    case SDLK_F8: return Key::F8;
    case SDLK_F9: return Key::F9;
    case SDLK_F10: return Key::F10;
    case SDLK_F11: return Key::F11;
    case SDLK_F12: return Key::F12;
    case SDLK_F13: return Key::F13;
    case SDLK_F14: return Key::F14;
    case SDLK_F15: return Key::F15;
    case SDLK_F16: return Key::F16;
    case SDLK_F17: return Key::F17;
    case SDLK_F18: return Key::F18;
    case SDLK_F19: return Key::F19;
    case SDLK_F20: return Key::F20;
    case SDLK_F21: return Key::F21;
    case SDLK_F22: return Key::F22;
    case SDLK_F23: return Key::F23;
    case SDLK_F24: return Key::F24;
    case SDLK_INSERT: return Key::NavInsert;
    case SDLK_DELETE: return Key::NavDelete;
    case SDLK_HOME: return Key::NavHome;
    case SDLK_END: return Key::NavEnd;
    case SDLK_PAGEUP: return Key::NavPgUp;
    case SDLK_PAGEDOWN: return Key::NavPgDown;
    case SDLK_PRINTSCREEN: return Key::PrintScreen;
    case SDLK_SCROLLLOCK: return Key::ScrollLock;
    case SDLK_PAUSE: return Key::Pause;
    case SDLK_NUMLOCKCLEAR: return Key::KpNumLock;
    case SDLK_KP_DIVIDE: return Key::KpDivide;
    case SDLK_KP_MULTIPLY: return Key::KpMultiply;
    case SDLK_KP_MINUS: return Key::KpSubtract;
    case SDLK_KP_PLUS: return Key::KpAdd;
    case SDLK_KP_ENTER: return Key::KpEnter;
    case SDLK_KP_0: return Key::Kp0;
    case SDLK_KP_1: return Key::Kp1;
    case SDLK_KP_2: return Key::Kp2;
    case SDLK_KP_3: return Key::Kp3;
    case SDLK_KP_4: return Key::Kp4;
    case SDLK_KP_5: return Key::Kp5;
    case SDLK_KP_6: return Key::Kp6;
    case SDLK_KP_7: return Key::Kp7;
    case SDLK_KP_8: return Key::Kp8;
    case SDLK_KP_9: return Key::Kp9;
    case SDLK_KP_PERIOD: return Key::KpDecimal;
    }
    log::warn("unhandled keycode: {}", keycode);
    return Key::CountDiscrim;
}

constexpr auto mapSdlMouseButtonToKey(u8 sdlMouseCode) -> Key {
    switch (sdlMouseCode) {
    case SDL_BUTTON_LEFT: return Key::MouseLeft;
    case SDL_BUTTON_RIGHT: return Key::MouseRight;
    case SDL_BUTTON_MIDDLE: return Key::MouseMiddle;
    case SDL_BUTTON_X1: return Key::Mouse4;
    case SDL_BUTTON_X2: return Key::Mouse5;
    }
    log::warn("unhandled mouse button code: {}", sdlMouseCode);
    return Key::CountDiscrim;
}

constexpr auto mapSdlKeyModToKeyMod(u32 sdlKeyMod) -> KeyModifierFlags {
    auto result = KeyModifierFlags::None;
    if (sdlKeyMod & SDL_KMOD_LSHIFT) result |= KeyModifierFlags::LShift;
    if (sdlKeyMod & SDL_KMOD_RSHIFT) result |= KeyModifierFlags::RShift;
    if (sdlKeyMod & SDL_KMOD_LCTRL) result |= KeyModifierFlags::LControl;
    if (sdlKeyMod & SDL_KMOD_RCTRL) result |= KeyModifierFlags::RControl;
    if (sdlKeyMod & SDL_KMOD_LALT) result |= KeyModifierFlags::LAlt;
    if (sdlKeyMod & SDL_KMOD_RALT) result |= KeyModifierFlags::RAlt;
    if (sdlKeyMod & SDL_KMOD_LGUI) result |= KeyModifierFlags::LSystem;
    if (sdlKeyMod & SDL_KMOD_RGUI) result |= KeyModifierFlags::RSystem;
    if (sdlKeyMod & SDL_KMOD_CAPS) result |= KeyModifierFlags::CapsLock;
    return result;
}

}