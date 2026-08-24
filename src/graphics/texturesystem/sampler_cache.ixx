module;
#include <xxhash.h>
#include <string.h>
export module projnekomata:graphics.texturesystem.sampler_cache;
import std;
import projnekomata.cs;
import vulkan;
import :graphics.vulkan.vk_sampler;

export namespace projnekomata::gfx {

struct SamplerParams {
    SamplerParams() = default;

    vk::Filter m_minFilter = vk::Filter::eLinear;
    vk::Filter m_magFilter = vk::Filter::eLinear;
    vk::SamplerMipmapMode m_mipmapMode = vk::SamplerMipmapMode::eLinear;
    vk::SamplerAddressMode m_addressModeU = vk::SamplerAddressMode::eClampToEdge;
    vk::SamplerAddressMode m_addressModeV = vk::SamplerAddressMode::eClampToEdge;
    vk::SamplerAddressMode m_addressModeW = vk::SamplerAddressMode::eClampToEdge;
    f32 m_anisotropy = 0.0f;
    f32 m_maxLod = vk::LodClampNone;
    f32 m_minLod = 0.0f;
    f32 m_mipLodBias = 0.0f;

    auto operator==(const SamplerParams& other) const -> bool {
        return memcmp(this, &other, sizeof(*this)) == 0;
    }

    [[nodiscard]] constexpr static auto defaultValues() -> SamplerParams {
        return SamplerParams();
    }

    [[nodiscard]] constexpr auto setMinFilter(vk::Filter filter) noexcept -> SamplerParams& {
        m_minFilter = filter;
        return *this;
    }
    [[nodiscard]] constexpr auto setMagFilter(vk::Filter filter) noexcept -> SamplerParams& {
        m_magFilter = filter;
        return *this;
    }
    [[nodiscard]] constexpr auto setMipmapMode(vk::SamplerMipmapMode mipmapMode) noexcept -> SamplerParams& {
        m_mipmapMode = mipmapMode;
        return *this;
    }
    [[nodiscard]] constexpr auto setAddressModeU(vk::SamplerAddressMode addressModeU) noexcept -> SamplerParams& {
        m_addressModeU = addressModeU;
        return *this;
    }
    [[nodiscard]] constexpr auto setAddressModeV(vk::SamplerAddressMode addressModeV) noexcept -> SamplerParams& {
        m_addressModeV = addressModeV;
        return *this;
    }
    [[nodiscard]] constexpr auto setAddressModeW(vk::SamplerAddressMode addressModeW) noexcept -> SamplerParams& {
        m_addressModeW = addressModeW;
        return *this;
    }
    [[nodiscard]] constexpr auto setAnisotropy(f32 anisotropy) noexcept -> SamplerParams& {
        m_anisotropy = anisotropy;
        return *this;
    }
    [[nodiscard]] constexpr auto setMaxLod(f32 maxLod) noexcept -> SamplerParams& {
        m_maxLod = maxLod;
        return *this;
    }
    [[nodiscard]] constexpr auto setMinLod(f32 minLod) noexcept -> SamplerParams& {
        m_minLod = minLod;
        return *this;
    }
    [[nodiscard]] constexpr auto setMipLodBias(f32 mipLodBias) noexcept -> SamplerParams& {
        m_mipLodBias = mipLodBias;
        return *this;
    }
};

struct SamplerParamsHash {
    static u64 hash(const SamplerParams& k) {
        return XXH3_64bits(&k, sizeof(k));
    }
};

class SamplerCache {
public:
    SamplerCache();

    SamplerCache(const SamplerCache&) = delete;
    SamplerCache(SamplerCache&&) = delete;
    SamplerCache& operator=(const SamplerCache&) = delete;
    SamplerCache& operator=(SamplerCache&&) = delete;

    /// Returns the shader index of the sampler. TODO: make a strong typedef
    [[nodiscard]] auto acquireSampler(const SamplerParams& params) -> u32;

private:
    struct SamplerCacheEntry {
        vkrhi::VulkanSampler sampler;
        u32 shaderIndex = 0;
    };

    static auto createSampler(const SamplerParams& params) -> SamplerCacheEntry;

    HashMap<SamplerParams, SamplerCacheEntry, SamplerParamsHash> m_hashmap = HashMap<SamplerParams, SamplerCacheEntry, SamplerParamsHash>::create();
    std::shared_mutex m_hashmapMutex;
};

}