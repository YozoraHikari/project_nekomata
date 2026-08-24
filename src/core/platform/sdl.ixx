module;
#include <SDL3/SDL.h>
export module projnekomata:core.platform.sdl;
import std;
import projnekomata.corelib;
import vulkan;
import :core.math;

export namespace projnekomata {

// TODO: Document
void sdlPlatformInit();
void sdlPlatformDeinit();

// TODO: Document
class SdlWindow {
public:
    SdlWindow(std::nullptr_t);
    SdlWindow(const std::string& title, uint32_t width, uint32_t height);
    ~SdlWindow();
    
    SdlWindow(const SdlWindow&) = delete;
    SdlWindow(SdlWindow&&) = default;
    SdlWindow& operator=(const SdlWindow&) = delete;
    SdlWindow& operator=(SdlWindow&&) = default;

    // TODO: Document
    [[nodiscard]] static auto vulkanInstanceExtensions() -> Vec<std::string>;
    // TODO: Document
    [[nodiscard]] auto vulkanCreateRawSurface(const vk::Instance& vkInstance) const -> vk::SurfaceKHR;
    // TODO: Document
    [[nodiscard]] auto vulkanGetDrawableSize() const -> vk::Extent2D;

    auto getLogicalSize() const -> math::Vector2i;
    auto getDisplayScale() const -> float;

    auto handle() const -> SDL_Window* { return m_window; }

private:
    SDL_Window* m_window = nullptr;
};

} // namespace projnekomata
