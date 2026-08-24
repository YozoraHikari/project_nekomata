export module projnekomata:core.runtime.mainthread;
import std;
import projnekomata.corelib;
import :core.runtime.shared_data;
import :graphics.vulkan.context;
import :core.platform.sdl;
import :core.ecs;
import :core.input.inputmanager;
import :graphics.meshsystem.mesh_asset_storage;
import :graphics.texturesystem.texture_manager;
import :graphics.fontsystem.font_manager;
import :core.ui.ui_system;
import :graphics.materialsystem.mat_manager;

export namespace projnekomata {

class MainThread {
public:
    MainThread(std::shared_ptr<MRThreadsSharedData> mrSharedData, Unique<gfx::vkrhi::VulkanContext>&& vkContext, SdlWindow&& sdlWindow);

    auto runMainLoop(const std::function<void(Unique<ecs::World>&)>&) -> void;
    auto getCurrentWorld() -> ecs::World*;

private:
    auto loop(float dt) -> void;

    auto updateEcsWorldTransforms() -> void;

    SdlWindow m_sdlWindow = nullptr;

    Unique<ecs::World> m_currentWorld = nullptr;

    std::shared_ptr<MRThreadsSharedData> m_mrSharedData = nullptr;
    Unique<core::input::Input> m_inputManager = nullptr;
    Unique<gfx::vkrhi::VulkanContext> m_vkContext = nullptr;
    Unique<gfx::MeshAssetStorage> m_meshAssetStorage = nullptr;
    Unique<gfx::TextureManager> m_textureManager = nullptr;
    Unique<gfx::MaterialManager> m_materialManager = nullptr;
    Unique<FontManager> m_fontManager = nullptr;


    Unique<ui::UiSystem> m_uiSystem = nullptr;

    u64 m_frameIndex = 0;
    bool m_waitForFrameStats = false;

    FontFace m_overlayFont;
};

}
