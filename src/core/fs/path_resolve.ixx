export module projnekomata:core.fs.path_resolve;
import std;

namespace projnekomata::fs {

struct Path {
    Path(const char* path) : trace(path) {}
    Path(const std::string& path) : trace(path) {}
    Path(std::string&& path) : trace(std::move(path)) {}
    Path(const std::filesystem::path& path) : trace(path.string()) {}
    Path(std::filesystem::path&& path) : trace(std::move(path).string()) {}

    std::string trace;

    auto string() const -> const std::string& { return trace; }
};

class PathResolver {
public:
    static auto resolve(const Path& path) -> std::filesystem::path;

    static auto setAssetsWorkingDirectory(const std::filesystem::path& path) -> void;
    static auto setSpirvWorkingDirectory(const std::filesystem::path& path) -> void;

private:
    static inline std::filesystem::path m_assetsWorkingDirectory = "";
    static inline std::filesystem::path m_spirvWorkingDirectory = "";
};

}