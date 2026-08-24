module;
#include <ktx.h>
export module projnekomata:graphics.texturesystem.texture_manager;
import std;
import vulkan;
import projnekomata.cs;
import :graphics.vulkan.vk_image;
import :graphics.srt.shader_resource_table;
import :core.containers.freelist_pool;
import :graphics.texturesystem.sampler_cache;
import :graphics.texturesystem.texture_to_shader_index_table;
import :core.fs.path_resolve;

export namespace projnekomata {

struct Texture {
    u32 index = 0;
};

}

export namespace projnekomata::gfx {

constexpr u32 kMaxTextureCount = 131072;
constexpr u32 kMaxSampledImageCount = 131072;
constexpr u32 kMaxSamplerCount = 4096;

inline class TextureManager* g_textureManager = nullptr;

struct TextureResources {
public:
    TextureResources(std::nullptr_t);
    TextureResources(vkrhi::VulkanImage&& image);

    [[nodiscard]] auto image() -> vkrhi::VulkanImage& { return m_image; }
    auto setImage(vkrhi::VulkanImage&& image) -> void { m_image = std::move(image); }

private:
    vkrhi::VulkanImage m_image = nullptr;

};

class TextureManager {
public:
    static auto get() -> TextureManager& { return *g_textureManager; }
    TextureManager(std::nullptr_t);
    TextureManager(Unique<IShaderResourceTable>&& srt);

    static auto create() -> Unique<TextureManager>;

    // auto loadTextureFromMemory(u32 width, u32 height, u32 depth, u32 arrayLayers, u32 mipLevels, vk::Format format, Slice<const u8>& data) -> Texture;

    auto createTexture(u32 width, u32 height, u32 depth, u32 layers, u32 mipLevels, bool isCube, vk::Format format, vk::ImageUsageFlags usage, const SamplerParams& samplerParams) -> Texture;

    /// Asynchronously loads a KTX2 texture from system storage.
    ///
    /// The texture can immediately be used, but it will be substituted for a dummy texture while the load is pending.
    auto loadKtx2TextureAsync(const fs::Path& path, const SamplerParams& samplerParams) -> Texture;
    auto loadKtx2TextureBlocking(const fs::Path& path, const SamplerParams& samplerParams) -> Texture;
    auto laodKtx2TextureFromMemoryBlocking(std::string name, Slice<const u8> data, const SamplerParams& samplerParams) -> Texture;
    auto freeTexture(Texture texture) -> void;

    [[nodiscard]] constexpr auto shaderResourceTable() -> IShaderResourceTable& { return *m_srt; }
    [[nodiscard]] constexpr auto samplerCache() -> SamplerCache& { return m_samplerCache; }
    [[nodiscard]] constexpr auto getTextureResources(Texture texture) -> TextureResources& { return m_loadedTextures[texture.index]; }
    [[nodiscard]] constexpr auto textureToShaderIndexTable() -> TextureToShaderIndexTable& { return m_textureToShaderIndexTable; }

private:

    auto allocateTexture(vkrhi::VulkanImage&& img) -> Texture;

    auto loadTextureFromMemoryInternal(u32 width, u32 height, u32 depth, u32 arrayLayers, u32 mipLevels, vk::Format format, Slice<const u8> data, const SamplerParams& samplerParams) -> Texture;

    static auto uploadKtx2Image(std::string name, Texture texture, ktxTexture2* ktxData, const SamplerParams& params) -> void;

    Texture m_defaultTexture;

    FreelistPoolV2<TextureResources, kMaxTextureCount> m_loadedTextures = nullptr;
    SamplerCache m_samplerCache;

    TextureToShaderIndexTable m_textureToShaderIndexTable = nullptr;

    Unique<IShaderResourceTable> m_srt = nullptr;
};

}