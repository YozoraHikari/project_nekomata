module projnekomata;
import :core.fs.path_resolve;

namespace projnekomata::fs {

auto PathResolver::resolve(const Path& path) -> std::filesystem::path {
    if (!path.trace.starts_with("//")) return path.trace;

    auto storeNameEnd = path.trace.find(":/");
    if (storeNameEnd == std::string::npos) return path.trace;

    auto storeName = path.trace.substr(2, storeNameEnd - 2);
    if (storeName == "spirv") return m_spirvWorkingDirectory / path.trace.substr(storeNameEnd + 2);
    if (storeName == "assets") return m_assetsWorkingDirectory / path.trace.substr(storeNameEnd + 2);

    panic("Unknown store name: {}", storeName);
}

auto PathResolver::setAssetsWorkingDirectory(const std::filesystem::path& path) -> void {
    m_assetsWorkingDirectory = path;
}

auto PathResolver::setSpirvWorkingDirectory(const std::filesystem::path& path) -> void {
    m_spirvWorkingDirectory = path;
}
}
