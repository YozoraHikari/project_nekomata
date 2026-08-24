export module projnekomata:core.runtime.shared_data;
import std;
import projnekomata.corelib;
import vulkan;
import :core.ecs.component_pool;
import :core.ecs.world.camera;
import :core.ecs.world.transform;
import :core.ecs.world.renderable;
import :core.containers.double_buffer;
import :core.ui.ui_drawcmds;
import :core.ecs.world.pointlight;
import :graphics.fontsystem.dynamic_font_atlas;

export namespace projnekomata {

struct MRThreadsSharedDataLeaf {
    MRThreadsSharedDataLeaf() = default;
    u64 m_frameIndex;
    vk::Extent2D m_currentWindowExtent;

    // ---- ECS Components -------------------------------------------------------------------------------------------------------------------------------------

    ecs::ComponentSetSnapshot<RenderableComponent> m_renderables;
    ecs::ComponentSetSnapshot<PointlightComponent> m_pointlights;
    ecs::ComponentSetSnapshot<WorldTransformComponent> m_transforms;
    ecs::ComponentSetSnapshot<CameraComponent> m_cameras;

    // ---- Bindless Textures ----------------------------------------------------------------------------------------------------------------------------------

    Vec<u32> m_textureToImageShaderIndexSnapshot   = Vec<u32>::filledWith(gfx::kMaxTextureCount, 0);
    Vec<u32> m_textureToSamplerShaderIndexSnapshot = Vec<u32>::filledWith(gfx::kMaxTextureCount, 0);

    // ---- Materials ------------------------------------------------------------------------------------------------------------------------------------------

    Vec<std::tuple<usize, Vec<u8>>> m_materialHeapSnapshotsBySize = Vec<std::tuple<usize, Vec<u8>>>::create();

    // ---- UI -------------------------------------------------------------------------------------------------------------------------------------------------

    Vec<ui::UiDrawCmd> m_uiDrawCmds = Vec<ui::UiDrawCmd>::create();

    Vec<u8>                                 m_fontsUploadPixelBuffer = Vec<u8>::create();
    HashMap<u32, Vec<vk::BufferImageCopy2>> m_fontsCopyRegions       = HashMap<u32, Vec<vk::BufferImageCopy2>>::create();
    Vec<u32>                                m_fontsNewImageIndices   = Vec<u32>::create();

    bool m_hasValidFrame = false;
    bool m_captureStats = false;
};

struct QueryTimestamps {
    u64 geomPassTopOfPipe;
    u64 geomPassBottomOfPipe;
    u64 lightingPassTopOfPipe;
    u64 lightingPassAfterDoneBottomOfPipe;
    u64 smaaTopOfPipe;
    u64 smaaBottomOfPipe;
};

class MRThreadsSharedData {
public:
    MRThreadsSharedData(vk::Extent2D windowCurrentRes);

    MRThreadsSharedData(const MRThreadsSharedData&) = delete;
    MRThreadsSharedData(MRThreadsSharedData&&) = delete;
    MRThreadsSharedData& operator=(const MRThreadsSharedData&) = delete;
    MRThreadsSharedData& operator=(MRThreadsSharedData&&) = delete;

    DoubleBuffer<MRThreadsSharedDataLeaf> m_leafs;
    std::barrier<> m_syncpointBarrier;

    std::atomic<bool> m_shouldQuit;
    std::string_view m_sdlVideoDriverName;
    gfx::DynamicBitmapFontAtlas m_fontAtlas;

    QueryTimestamps m_queryTimestamps;
    u64 m_deferredGeometryPipelineStats[4];
    f32 m_deltaTime;
    u32 m_numDrawcalls;

    bool m_queryPoolStatsAreValid = false;
    std::atomic<bool> m_statsReady = false;
};

}
