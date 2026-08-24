module projnekomata;
import :graphics.texturesystem.texture_manager;

namespace projnekomata::gfx {

SamplerCache::SamplerCache() {}

auto SamplerCache::acquireSampler(const SamplerParams& params) -> u32 {
    {
        std::shared_lock lock(m_hashmapMutex);
        if (auto entry = m_hashmap.get(params)) return entry.unwrap().get().shaderIndex;
    }

    std::unique_lock lock(m_hashmapMutex);
    if (auto entry = m_hashmap.get(params)) return entry.unwrap().get().shaderIndex;

    m_hashmap.insert(params, createSampler(params));
    return m_hashmap[params].shaderIndex;
}

auto SamplerCache::createSampler(const SamplerParams& params) -> SamplerCacheEntry {
    auto sampler = vkrhi::VulkanSampler::create(params.m_minFilter, params.m_magFilter, params.m_mipmapMode, params.m_addressModeU, params.m_addressModeV, params.m_addressModeW, params.m_anisotropy > 1.0f, params.m_anisotropy, params.m_minLod, params.m_maxLod, params.m_mipLodBias);

    auto idx = TextureManager::get().shaderResourceTable().allocateSamplerIndex();
    TextureManager::get().shaderResourceTable().bindSampler(sampler, idx);

    auto entry = SamplerCacheEntry{std::move(sampler), idx.imageIndex};

    return entry;
}

} // namespace projnekomata