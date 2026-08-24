module;
#include <SDL3/SDL_messagebox.h>
module projnekomata;
import std;
import projnekomata.corelib;
import :core.platform.sdl;
import :graphics.vulkan.context;
import :core.runtime.shared_data;
import :core.runtime.mainthread;
import :core.runtime.graphicsthread;
import :core.sfconfig;
import :core.fs.exec_path;
import :core.fs.fs_quickbits;

namespace projnekomata {

auto entryAfterSdlInit(const std::function<void(Unique<ecs::World>&)>& initFn) -> void {
    Thread::setThreadName("MainThread");
    // TODO: A system to remember player preferences to use here instead.

    auto execPath = projnekomata::fs::executablePath();
    auto manifestPath = execPath.replace_extension(".manifest");
    auto manifestContents = projnekomata::fs::readToStringSystem(manifestPath);
    auto manifest = sfconfig::ConfigData::parse(manifestContents);

    // TODO: improve ergonomics of this
    auto assetsPath = manifest.get("manifest").unwrap().get()["assetspath"].unwrap().get().asString();
    auto spirvPath = manifest.get("manifest").unwrap().get()["spirvpath"].unwrap().get().asString();
    projnekomata::fs::PathResolver::setAssetsWorkingDirectory(assetsPath);
    projnekomata::fs::PathResolver::setSpirvWorkingDirectory(spirvPath);

    auto window = projnekomata::SdlWindow("Project Nekomata", 1920, 1080);

    // NOTE: This creates the vk::SurfaceKHR so this MUST be called here to respect SDL thread safety rules. Creating the renderer on the graphics thread is
    // NOTE: fine though.
    Unique<gfx::vkrhi::VulkanContext> vulkanContext = gfx::vkrhi::VulkanContext::create(window);
    auto windowCurrentRes = window.vulkanGetDrawableSize();

    auto threadSharedData = std::make_shared<MRThreadsSharedData>(windowCurrentRes);
    threadSharedData->m_sdlVideoDriverName = SDL_GetCurrentVideoDriver();

    MainThread mainthread(threadSharedData, std::move(vulkanContext), std::move(window));

    auto renderThreadHandle = std::thread([&]() {
        RenderThread renderThread(threadSharedData);
        Thread::setThreadName("RenderThread");
        renderThread.runMainLoop();
    });



    mainthread.runMainLoop(initFn);
    renderThreadHandle.join();
}

auto entry(const std::function<void(Unique<ecs::World>&)>& initFn) -> void {
    setupBacktrace();
    sdlPlatformInit();
    entryAfterSdlInit(initFn);
    sdlPlatformDeinit();
}

}
