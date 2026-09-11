module projnekomata;
import fmt;
import projnekomata.corelib;
import :graphics.cmd_alloc;
import :graphics.vulkan.context;
import :graphics.meshsystem.mesh_asset_storage;
import :core.runtime.graphicsthread;
import :graphics.fontsystem.font_manager;

namespace projnekomata {

RenderThread::RenderThread(const std::shared_ptr<MRThreadsSharedData>& mrSharedData)
    : m_mrSharedData(mrSharedData) {
        m_currentWindowExtent = mrSharedData->m_leafs.getSecondary().m_currentWindowExtent;
    }

static auto& cvRdVsync = CvarManager::get().registerCvar<bool>("rd.vsync", false);

auto RenderThread::runMainLoop() -> void {
    gfx::vkrhi::VulkanCommandPoolsList::initThreadLocalCommandPools();

    m_vkSwapchain = gfx::vkrhi::VulkanSwapchain::create(m_currentWindowExtent, None, cvRdVsync.get());
    m_currentWindowExtent = m_vkSwapchain.imageExtent();
    // TODO : remove the abuse
    std::construct_at(&m_sharedRenderingResources);
    m_transientRenderingResources = gfx::TransientRenderingResources(m_currentWindowExtent, m_sharedRenderingResources);
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_frames[i] = gfx::FrameContext();
    }

    m_timeAtStart = std::chrono::high_resolution_clock::now();

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    while (true) {
        m_mrSharedData->m_syncpointBarrier.arrive_and_wait();

        if (m_mrSharedData->m_shouldQuit.load(std::memory_order_acquire)) {
            break;
        }
        // NOTE: The main thread is responsible for swapping the data buffer leafs.

        m_mrSharedData->m_statsReady.store(false, std::memory_order_release);

        m_mrSharedData->m_syncpointBarrier.arrive_and_wait();

        loop();
    }

    log::info("Render Thread exiting...");

    gfx::vkrhi::VulkanCommandPoolsList::destroyThreadLocalCommandPools();
}


auto RenderThread::loop() -> void {
    if (!m_mrSharedData->m_leafs.getSecondary().m_hasValidFrame) return;

    auto currentTime = std::chrono::high_resolution_clock::now();
    auto deltaTime = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
    m_lastFrameTime = currentTime;

    fpsSmoothedDt = fpsDtSmoothAlpha * deltaTime + (1.0f - fpsDtSmoothAlpha) * fpsSmoothedDt;

    auto timeSinceLastFpsReport = std::chrono::duration<float>(currentTime - m_lastFpsReportTime).count();

    bool shouldReportFps = timeSinceLastFpsReport > 0.4f;
    if (shouldReportFps) {
        m_lastFpsReportTime = currentTime;
        m_sharedRenderingResources.displayMs = fpsSmoothedDt * 1000.0f;
    }

    m_sharedRenderingResources.checkGraphicsSettingsAndMaybeRecompileShaders();
    gfx::MeshAssetStorage::get().tickGC(m_currentFrameNumber);

    auto maybeNewWindowExtent = m_mrSharedData->m_leafs.getSecondary().m_currentWindowExtent;
    auto vsyncStatusChanged = cvRdVsync.get() != m_vkSwapchain.hasVsync();
    if (m_mustRecreateSwapchainNextFrame || m_currentWindowExtent != maybeNewWindowExtent || vsyncStatusChanged) {
        m_mustRecreateSwapchainNextFrame = false;

        // TODO: This is to work around present queues not being friendly to synchronize
        gfx::vkrhi::VulkanContext::get().vkDevice().waitIdle();
        auto oldSwapchain = std::move(m_vkSwapchain);
        m_vkSwapchain = gfx::vkrhi::VulkanSwapchain::create(m_currentWindowExtent, Some(std::move(oldSwapchain)), cvRdVsync.get());
        m_currentWindowExtent = m_vkSwapchain.imageExtent();
        m_transientRenderingResources.handleWindowSizeChange(m_currentWindowExtent);
    }

    m_frames[m_currentFrameContextIndex].waitForLastFrame();

    if (m_mrSharedData->m_leafs.getSecondary().m_captureStats) {
        bool hasStats = m_frames[m_currentFrameContextIndex].m_queryPoolsHaveResultsOnFinish;
        bool supportsPipelineStatisticsQuery = gfx::vkrhi::VulkanContext::get().vkPhysicalDeviceProps().m_hasPipelineStatisticsQuery;
        m_mrSharedData->m_queryPoolStatsAreValid = hasStats;

        if (hasStats) {
            auto timestampCount = static_cast<u32>(gfx::FrameContextTimestampIndex::CountDiscrim);
            gfx::vkrhi::vkCheckResult(m_frames[m_currentFrameContextIndex].m_timestampsQueryPool.vkQueryPool().getResults(0, timestampCount, timestampCount * sizeof(u64), &m_mrSharedData->m_queryTimestamps, 8, vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait));
            if (supportsPipelineStatisticsQuery) gfx::vkrhi::vkCheckResult(m_frames[m_currentFrameContextIndex].m_pipelineStatisticsQueryPool.vkQueryPool().getResults(0, 1, 32, &m_mrSharedData->m_deferredGeometryPipelineStats, 8, vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait));
        }

        m_mrSharedData->m_deltaTime = m_sharedRenderingResources.displayMs;
        m_mrSharedData->m_numDrawcalls = m_frames[m_currentFrameContextIndex].m_numDrawcalls;

        m_mrSharedData->m_statsReady.store(true, std::memory_order_release);
        m_mrSharedData->m_statsReady.notify_one();
    }

    bool shouldCaptureStats = m_mrSharedData->m_leafs.getSecondary().m_captureStats;
    auto result = m_frames[m_currentFrameContextIndex].execute(m_transientRenderingResources, m_sharedRenderingResources, m_vkSwapchain,
                                                               m_mrSharedData->m_leafs.getSecondary(), *m_mrSharedData, shouldCaptureStats);
    if (result.stepPerFrameResources)
        m_currentFrameContextIndex = (m_currentFrameContextIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    if (result.shouldRecreateSwapchain)
        m_mustRecreateSwapchainNextFrame = true;

    m_currentFrameNumber++;
}

}