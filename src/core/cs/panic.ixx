module;
#include <stdlib.h>
#include <unistd.h>
#if defined(__linux__)
#include <execinfo.h>

#if defined(PROJNEKOMATA_USE_LIBBACKTRACE)
#include <backtrace.h>
#include <cxxabi.h>
#endif

#endif
export module projnekomata.cs:panic;
import std;
import fmt;
import :log;
import :primitives;
import :thread;

constexpr bool kPanicPrintsStackTrace = true;

#if defined(PROJNEKOMATA_USE_LIBBACKTRACE)
inline backtrace_state* state;
#endif

export inline void setupBacktrace() {
#if defined(PROJNEKOMATA_USE_LIBBACKTRACE)
    state = backtrace_create_state(
        nullptr,
        1,
        nullptr,
        nullptr);
#endif
}

#if defined(PROJNEKOMATA_USE_LIBBACKTRACE)
inline std::string demangleCxxName(const char* name) {
    if (!name) return "[unknown function]";

    int status = 0;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);

    std::string result = (status == 0 && demangled) ? demangled : name;

    free(demangled);
    return result;
}
int backtracePrintlineCallback(void* data, uintptr_t pc, const char* filename, i32 lineno, const char* function) {
    projnekomata::log::crit("  #{}: {}",
        *static_cast<i32*>(data), function ? demangleCxxName(function) : "[unknown function]"
    );
    projnekomata::log::crit("    at {}:{}", filename ? filename : "[unknown source]", lineno);
    return 0;
}
void backtraceErrorCallback(void* data, const char* errorMsg, int errnum) {
    projnekomata::log::crit("  #{}: (failed to resolve: {})", *static_cast<i32*>(data), errorMsg);
}
#endif

[[noreturn]]
void panic_impl(std::source_location loc, fmt::string_view format, fmt::format_args args) {
    projnekomata::log::crit("thread {} (id {}) panicked at {}:{}: {}", Thread::getThreadName(), Thread::getThreadId(), loc.file_name(), loc.line(), fmt::vformat(format, args));

#if defined(__linux__) && defined(PROJNEKOMATA_USE_LIBBACKTRACE)
    if constexpr (kPanicPrintsStackTrace) {
        void* stack[128];
        i32 frameCount = backtrace(stack, 128);

        projnekomata::log::crit("stack trace:");
        for (i32 i = 0; i < frameCount; i++) {
            backtrace_pcinfo(
                state,
                (uintptr_t)stack[i],
                backtracePrintlineCallback,
                backtraceErrorCallback,
                static_cast<void*>(&i)
            );
        }
    } else {
        projnekomata::log::crit("stack trace printing was disabled by kPanicPrintsStackTrace");
    }
#else
    projnekomata::log::crit("stack trace printing is not supported");
#endif

    abort();
}

export template<typename... Args>
struct panic {
    [[noreturn]] panic(fmt::format_string<Args...> format,
          Args&&... args,
          std::source_location loc = std::source_location::current())
    {
        panic_impl(loc, format.get(), fmt::make_format_args(args...));
    }
};

export template<typename... Args>
[[noreturn]] panic(fmt::format_string<Args...>, Args&&...) -> panic<Args...>;