export module projnekomata:core.fs.fs_quickbits;
import std;
import projnekomata.cs;
import :core.fs.path_resolve;

export namespace projnekomata::fs {

auto readToStringSystem(const std::filesystem::path& path) -> std::string {
    std::ifstream file(path);
    if (!file.is_open()) {
        panic("failed to open file {}", path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

auto readToString(const Path& path) -> std::string {
    auto pa = PathResolver::resolve(path);
    return readToStringSystem(pa);
}

}
