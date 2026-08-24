module;
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif
export module projnekomata.corelib:thread;
import std;
import :primitives;

export class Thread {
public:
#if defined(_WIN32)
    static inline void setThreadName(const std::string& name) {
        tl_threadName = name;
        std::wstring name_wstring(name.begin(), name.end());
        SetThreadDescription(GetCurrentThread(), name_wstring.c_str());
    }
#elif defined(__APPLE__)
    static inline void setThreadName(const std::string& name) {
        tl_threadName = name;
        pthread_setname_np(name.c_str());
    }
#elif defined(__linux__)
    static inline void setThreadName(const std::string& name) {
        tl_threadName = name;
        pthread_setname_np(pthread_self(), name.c_str());
    }
#else
    static inline void setThreadName(const std::string& name) {
        tl_threadName = name;
        // Platform thread name isn't supported
    }
#endif

    static inline auto getThreadName() -> const std::string& {
        return tl_threadName;
    }

    static inline auto getThreadId() -> u64 {
#if defined(_WIN32)
        return static_cast<u64>(GetCurrentThreadId());
#elif defined(__linux__)
        return static_cast<u64>(syscall(SYS_gettid));
#elif defined(__APPLE__)
        return static_cast<u64>(pthread_mach_thread_np(pthread_self()));
#endif
        return 0;
    }
private:

    static inline thread_local std::string tl_threadName = "(unnamed thread)";
};