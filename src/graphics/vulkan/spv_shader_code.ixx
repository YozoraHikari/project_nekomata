export module projnekomata:graphics.vulkan.spv_shader_code;
import std;
import projnekomata.cs;
import vulkan;
import :core.fs.path_resolve;

export namespace projnekomata::gfx::vkrhi {

enum class ShaderLoadError {
    FileLoadingError,
    FileReadingError,
    MisalignedFileSize,
};

class SpirvShaderCode {
public:
    SpirvShaderCode(std::nullptr_t);
    SpirvShaderCode(Vec<u32>&& spvCode);

    SpirvShaderCode(const SpirvShaderCode&) = delete;
    SpirvShaderCode(SpirvShaderCode&&) = default;
    SpirvShaderCode& operator=(const SpirvShaderCode&) = delete;
    SpirvShaderCode& operator=(SpirvShaderCode&&) = default;

    static auto loadFromFile(const fs::Path& path) -> Result<SpirvShaderCode, ShaderLoadError>;
    [[nodiscard]] auto shaderModuleCreateInfo() const           -> vk::ShaderModuleCreateInfo;

    [[nodiscard]] auto spvCode() const -> Slice<const u32> { return m_spvCode.asSlice(); }
private:
    Vec<u32> m_spvCode;
};

}
