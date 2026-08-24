module projnekomata;
import projnekomata.cs;
import :graphics.vulkan.spv_shader_code;

namespace projnekomata::gfx::vkrhi {

SpirvShaderCode::SpirvShaderCode(std::nullptr_t) {}
SpirvShaderCode::SpirvShaderCode(Vec<u32>&& spvCode) : m_spvCode(std::move(spvCode)) {}

auto SpirvShaderCode::loadFromFile(const fs::Path& path) -> Result<SpirvShaderCode, ShaderLoadError> {
    log::info("loading file: {}", path.string());
    auto resolvedPath = fs::PathResolver::resolve(path);
    std::ifstream shaderCodeFile(resolvedPath, std::ios::binary | std::ios::ate);
    if (!shaderCodeFile) {
        log::crit("Failed to read shader SPIR-V code at {}: cannot access file", path.string());
        return Err(ShaderLoadError::FileLoadingError);
    }
    
    auto fileSize = shaderCodeFile.tellg();
    
    if ((fileSize & 0b11) != 0) {
        log::crit("Failed to read shader SPIR-V code at {}: file size is not a multiple of 4", path.string());
        return Err(ShaderLoadError::MisalignedFileSize);
    }

    auto shaderCodeDwordCount = fileSize / 4;
    auto spvCode = Vec<u32>::filledWith(shaderCodeDwordCount, 0);

    shaderCodeFile.seekg(0);
    shaderCodeFile.read(reinterpret_cast<char*>(spvCode.data()), fileSize);
    if (!shaderCodeFile) {
        log::crit("Failed to read shader SPIR-V code at {}: could not read all dwords", path.string());
        return Err(ShaderLoadError::FileReadingError);
    }

    return Ok(SpirvShaderCode(std::move(spvCode)));
}

auto SpirvShaderCode::shaderModuleCreateInfo() const -> vk::ShaderModuleCreateInfo {
    return vk::ShaderModuleCreateInfo{}
        .setCode(m_spvCode);
}

}