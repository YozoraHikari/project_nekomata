export module projnekomata:graphics.texturesystem.texture_to_shader_index_table;
import std;
import projnekomata.cs;

export namespace projnekomata::gfx {

/// The reason why we need to translate texture indices to shader indices is as follows:
///  - Amendments of images/image views (necessary for mip streaming).
///  - It's easier to bring up a GPU-driven pipeline based on this rather than twiddling with ready-flags and whatnot.
///
/// Snapshots of the tables must be made instead of consuming live values to avoid the next main loop iteration messing with values during the current render loop iteration.
class TextureToShaderIndexTable {
public:
    TextureToShaderIndexTable(std::nullptr_t);
    TextureToShaderIndexTable(usize maxTextureCount);

    TextureToShaderIndexTable(const TextureToShaderIndexTable&) = delete;
    TextureToShaderIndexTable(TextureToShaderIndexTable&&) = delete;
    TextureToShaderIndexTable& operator=(const TextureToShaderIndexTable&) = delete;
    TextureToShaderIndexTable& operator=(TextureToShaderIndexTable&&) = delete;

    auto setTextureShaderImageIndex(usize textureId, u32 shaderImageIndex) -> void;
    auto setTextureShaderSamplerIndex(usize textureId, u32 shaderSamplerIndex) -> void;

    [[nodiscard]] auto textureToShaderImageIndex(usize textureId) const -> u32;
    [[nodiscard]] auto textureToShaderSamplerIndex(usize textureId) const -> u32;

    auto snapshotTables(Vec<u32>& dstTextureToShaderImageIndexTable, Vec<u32>& dstTextureToShaderSamplerIndexTable) const -> void;
private:
    Vec<std::atomic<u32>> m_textureToShaderImageIndexTable   = Vec<std::atomic<u32>>::create();
    Vec<std::atomic<u32>> m_textureToShaderSamplerIndexTable = Vec<std::atomic<u32>>::create();
};

}