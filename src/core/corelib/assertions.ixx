export module projnekomata.corelib:assertions;
import :panic;

export constexpr void debug_assert(const bool& condition, std::string_view message, std::source_location loc = std::source_location::current()) {
#ifdef NDEBUG
    __builtin_assume(condition);
    return;
#endif
    if (!condition) {
        panic("assertion failed at {}:{}: {}", loc.file_name(), loc.line(), message);
    }

    __builtin_assume(condition);
}