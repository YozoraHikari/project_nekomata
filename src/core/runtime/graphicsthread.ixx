export module projnekomata:core.runtime.graphicsthread;
import std;
import projnekomata.cs;
import vulkan;
import :core.runtime.shared_data;
import :graphics.vulkan.vk_swapchain;
import :graphics.rendering.transient_rendering_resources;
import :graphics.rendering.shared_rendering_resources;
import :graphics.rendering.frame_context;

export namespace projnekomata {

constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

class RenderThread {
public:
    RenderThread(const std::shared_ptr<MRThreadsSharedData>& mrSharedData);

    auto runMainLoop() -> void;

private:
    auto loop() -> void;

    gfx::vkrhi::VulkanSwapchain m_vkSwapchain = nullptr;
    gfx::SharedRenderingResources m_sharedRenderingResources = nullptr;
    gfx::TransientRenderingResources m_transientRenderingResources = nullptr;
    std::array<gfx::FrameContext, MAX_FRAMES_IN_FLIGHT> m_frames = {nullptr};
    usize m_currentFrameContextIndex = 0;
    usize m_currentFrameNumber = 0;

    // with MainThread
    std::shared_ptr<MRThreadsSharedData> m_mrSharedData = nullptr;
    bool m_mustRecreateSwapchainNextFrame = false;
    vk::Extent2D m_currentWindowExtent;

    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFrameTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFpsReportTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_timeAtStart;

    float fpsSmoothedDt = 0.0f;
    float fpsDtSmoothAlpha = 0.05f;
};

}