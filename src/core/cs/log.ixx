module;
#include <ctime>
export module projnekomata.cs:log;
import std;
import fmt;

export namespace projnekomata::log {

enum class LogLevel { Trace, Info, Warn, Error, Crit };

namespace impl {

namespace ansi_codes {
// Background Colors
inline constexpr std::string_view RESET = "\033[0m";
inline constexpr std::string_view BOLD = "\033[1m";
inline constexpr std::string_view DIM = "\033[2m";

// Foreground Colors
inline constexpr std::string_view NONE;
inline constexpr std::string_view WHITE = "\033[97m";
inline constexpr std::string_view CYAN = "\033[96m";
inline constexpr std::string_view GREEN = "\033[92m";
inline constexpr std::string_view YELLOW = "\033[93m";
inline constexpr std::string_view RED = "\033[91m";
inline constexpr std::string_view MAGENTA = "\033[95m";

// Background color used for CRIT
inline constexpr std::string_view BRED = "\033[41m";

// Combinations used for the log level coloring
inline constexpr std::string_view DIM_CYAN = "\033[2m\033[96m";
inline constexpr std::string_view BOLD_YELLOW = "\033[1m\033[93m";
inline constexpr std::string_view BOLD_RED = "\033[1m\033[91m";
inline constexpr std::string_view BOLD_WHITE_ON_RED = "\033[1m\033[41m\033[97m";

} // namespace ansi_codes

struct LevelMeta {
    std::string_view m_label;
    std::string_view m_color;
    std::string_view m_prefix;
};

inline constexpr LevelMeta meta(LogLevel l) noexcept {
    using namespace ansi_codes;
    switch (l) {
    case LogLevel::Trace:
        return {"TRACE", NONE, DIM_CYAN};
    case LogLevel::Info:
        return {" INFO", NONE, GREEN};
    case LogLevel::Warn:
        return {" WARN", YELLOW, BOLD_YELLOW};
    case LogLevel::Error:
        return {"ERROR", RED, BOLD_RED};
    case LogLevel::Crit:
        return {" CRIT", RED, BOLD_WHITE_ON_RED};
    }
    return {};
}

inline std::string timestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto time = floor<seconds>(now);
    const auto us = duration_cast<microseconds>(now - time).count();
    const std::time_t tt = system_clock::to_time_t(time);

    std::tm buf{};
#if defined(_WIN32)
    localtime_s(&buf, &tt);
#else
    localtime_r(&tt, &buf);
#endif
    return fmt::format("{:02}:{:02}:{:02}.{:06}", buf.tm_hour, buf.tm_min, buf.tm_sec, us);
}

inline void write(LogLevel level, fmt::string_view fmtstr, fmt::format_args args) {
    using namespace ansi_codes;
    const auto& m = meta(level);

    std::osyncstream stream(std::cout);


    stream << DIM << WHITE << '[' << timestamp() << ']' << RESET
        << ' ' << m.m_prefix << ' ' << m.m_label << ' ' << RESET
        << " " << m.m_color;

    fmt::vformat_to(
        std::ostreambuf_iterator<char>(stream),
        fmtstr,
        args
    );

    stream << RESET << '\n';
}

} // namespace impl

template <typename... Args> void trace(fmt::format_string<Args...> fmtstr, Args&&... args) {
    impl::write(LogLevel::Trace, fmtstr.get(), fmt::make_format_args(args...));
}

template <typename... Args> void info(fmt::format_string<Args...> fmtstr, Args&&... args) {
    impl::write(LogLevel::Info, fmtstr.get(), fmt::make_format_args(args...));
}

template <typename... Args> void warn(fmt::format_string<Args...> fmtstr, Args&&... args) {
    impl::write(LogLevel::Warn, fmtstr.get(), fmt::make_format_args(args...));
}

template <typename... Args> void error(fmt::format_string<Args...> fmtstr, Args&&... args) {
    impl::write(LogLevel::Error, fmtstr.get(), fmt::make_format_args(args...));
}

template <typename... Args> void crit(fmt::format_string<Args...> fmtstr, Args&&... args) {
    impl::write(LogLevel::Crit, fmtstr.get(), fmt::make_format_args(args...));
}

}