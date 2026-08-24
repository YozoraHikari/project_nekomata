module;
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
export module projnekomata:core.fs.exec_path;
import projnekomata.cs;
import std;

namespace projnekomata::fs {

auto executablePath() -> std::filesystem::path {
#if defined(_WIN32)
    std::wstring path(MAX_PATH, L'\0');
    GetModuleFileNameW(nullptr, &path[0], MAX_PATH);
    return std::filesystem::path(path);
#elif defined(__linux__)
    char buffer[PATH_MAX];
    auto len = readlink("/proc/self/exe", buffer, PATH_MAX);
    return std::filesystem::path(std::string(buffer, len));
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(&path[0], &size) != 0) panic("failed to get executable path");
    return std::filesystem::canonical(std::filesystem::path(path));
#else
#error "Unsupported platform"
#endif
}

}