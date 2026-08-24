module;
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>
module projnekomata;
import std;
import projnekomata.corelib;
import vulkan;
import :core.math;
import :core.platform.sdl;

namespace projnekomata {

void sdlPlatformInit() {
    log::info("Initializing SDL...");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        panic("Failed to initialize SDL: {}", SDL_GetError());
    }

    auto sdlVideodriver = SDL_GetCurrentVideoDriver();
    log::info("Current SDL video driver: {}", sdlVideodriver);
}
void sdlPlatformDeinit() { SDL_QuitSubSystem(SDL_INIT_VIDEO); SDL_Quit(); }

SdlWindow::SdlWindow(std::nullptr_t) {}

SdlWindow::SdlWindow(const std::string& title, const uint32_t width, const uint32_t height) {
    m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (m_window == nullptr) {
        panic("failed to create SDL window: {}", SDL_GetError());

    }
}

SdlWindow::~SdlWindow() { log::info("destroying window..."); SDL_DestroyWindow(m_window); }

auto SdlWindow::vulkanInstanceExtensions() -> Vec<std::string> {
    uint32_t extensionCount;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    auto result = Vec<std::string>::withCapacity(extensionCount);
    for (uint32_t i = 0; i < extensionCount; i++) {
        result.emplace(extensions[i]);
    }

    return result;
}

auto SdlWindow::vulkanCreateRawSurface(const vk::Instance& vkInstance) const -> vk::SurfaceKHR {
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(m_window, vkInstance, nullptr, &surface)) {
        auto msg = SDL_GetError();
        panic("SDL error when creating vk::SurfaceKHR: {}", msg);
    }

    return {surface};
}

auto SdlWindow::vulkanGetDrawableSize() const -> vk::Extent2D {
    vk::Extent2D extents;
    SDL_GetWindowSizeInPixels(m_window, reinterpret_cast<i32*>(&extents.width), reinterpret_cast<i32*>(&extents.height));
    return extents;
}

auto SdlWindow::getLogicalSize() const -> math::Vector2i {
    math::Vector2i size;
    SDL_GetWindowSize(m_window, &size.x(), &size.y());
    return size;
}

auto SdlWindow::getDisplayScale() const -> float {
    return SDL_GetWindowDisplayScale(m_window);
}

} // namespace projnekomata

