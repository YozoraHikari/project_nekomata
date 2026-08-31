export module projnekomata:core.input.inputmanager;
import :core.input.keys;
import :core.input.mousemodes;
import :core.platform.sdl;

export namespace projnekomata::core::input {

inline class Input* g_input = nullptr;

struct InputKeyEvent {
    Key key;
    KeyModifierFlags modifiers;
    bool state;
    bool isRepeat;
};

class Input {
public:
    static auto create() -> Unique<Input>;
    static auto get() -> Input& { return *g_input; }

    auto handleNewFrame(SdlWindow& window) -> void;
    auto insertInputKeyEvent(InputKeyEvent&& event) -> void;
    auto setKeyState(Key x, bool state) -> void;
    auto setMousePosition(math::Vector2f pos) -> void;
    auto setMouseDelta(math::Vector2f delta) -> void;
    
    auto setMouseMode(MouseMode mode) -> void;
    auto getMouseMode() const -> MouseMode { return m_mouseMode; }
    auto getSystemCurrentMouseMode() const -> MouseMode { return m_systemCurrentMouseMode; }
    auto isKeyPressed(Key x) const -> bool;
    auto isKeyReleased(Key x) const -> bool;
    auto isKeyDown(Key x) const -> bool;
    auto keyEventsThisFrame() const -> const Vec<InputKeyEvent>& { return m_keyEventsThisFrame; }
    auto mousePosition() const -> math::Vector2f;
    auto mouseDelta() const -> math::Vector2f;

private:
    u32 m_thisFramePressedKeyCounts[kKeyDiscrimsCount];
    u32 m_thisFrameReleasedKeyCounts[kKeyDiscrimsCount];
    bool m_keyStates[kKeyDiscrimsCount];

    Vec<InputKeyEvent> m_keyEventsThisFrame = Vec<InputKeyEvent>::create();

    MouseMode m_mouseMode              = MouseMode::Normal;
    MouseMode m_systemCurrentMouseMode = MouseMode::Normal;

    math::Vector2f m_mousePosition = math::Vector2f::zero();
    math::Vector2f m_mouseDelta    = math::Vector2f::zero();
};

}