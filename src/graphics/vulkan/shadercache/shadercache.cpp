module projnekomata;
import :graphics.vulkan.shadercache;

namespace projnekomata::gfx::vkrhi {

ShaderCache::ShaderCache(bool usePipelineBinaries) {
    if (usePipelineBinaries) {
        m_shaderCacheFrontend = ShaderCachePipelineBinaryFrontend(makeShaderCacheDirectoryPath(), 2, 1);
    }

    match(m_shaderCacheFrontend,
        [&](ShaderCachePipelineBinaryFrontend& sc) { sc.checkGlobalKeyAndInvalidateStale(); },
        [&](auto&) { }
    );
}

auto ShaderCache::makeShaderCacheDirectoryPath() -> std::filesystem::path {
    // TODO: use a proper path
    return "./shadercache_placeholder_name/";
}

} // namespace projnekomata::gfx::vkrhi