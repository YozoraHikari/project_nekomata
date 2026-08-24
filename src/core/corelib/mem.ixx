module;
#include <cstdlib>
#include <malloc.h>
#if defined(__linux__)
#include <sys/mman.h>
#elif defined(_WIN32)
#include <memoryapi.h>
#else
#error "Unsupported platform"
#endif
export module projnekomata.corelib:mem;
import :panic;
import :primitives;

export class Mem {
public:
    // ---- Allocation -----------------------------------------------------------------------------------------------------------------------------------------
    template <typename T> static auto alloc(usize len) -> T* { return static_cast<T*>(::malloc(len * sizeof(T))); }
    template <typename T> static auto allocAligned(usize len, usize alignment) -> T* { return static_cast<T*>(::aligned_alloc(alignment, len * sizeof(T))); }
    template <typename T> static auto realloc(T* ptr, usize len) -> T* { return static_cast<T*>(::realloc(static_cast<void*>(ptr), len * sizeof(T))); }

    // ---- Ensured Allocation ---------------------------------------------------------------------------------------------------------------------------------
    template <typename T> static auto allocChecked(usize len) -> T* {
        auto ptr = alloc<T>(len);
        if (!ptr) panic("out of host memory");
        return ptr;
    }
    template <typename T> static auto allocAlignedChecked(usize len, usize alignment) -> T* {
        auto ptr = allocAligned<T>(len, alignment);
        if (!ptr) panic("out of host memory");
        return ptr;
    }
    template <typename T> static auto reallocChecked(T* ptr, usize len) -> T* {
        auto ptr2 = realloc<T>(ptr, len);
        if (!ptr2) panic("out of host memory");
        return ptr2;
    }

    // ---- Virtual Allocations --------------------------------------------------------------------------------------------------------------------------------
    template <typename T> static auto vmReserve(usize len) -> T* {
#if defined(__linux__)
        void* ptr = ::mmap(nullptr, len * sizeof(T), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return ptr == MAP_FAILED ? nullptr : static_cast<T*>(ptr);
#elif defined(_WIN32)
        return static_cast<T*>(::VirtualAlloc(nullptr, len * sizeof(T), MEM_RESERVE, PAGE_NOACCESS));
#endif
    }

    static auto vmCommit(void* ptr, usize len) -> void {
#if defined(__linux__)
        ::mprotect(ptr, len, PROT_READ | PROT_WRITE);
#elif defined(_WIN32)
        ::VirtualAlloc(ptr, len, MEM_COMMIT, PAGE_READWRITE);
#endif
    }

    static auto vmUncommit(void* ptr, usize len) -> void {
#if defined(__linux__)
        ::madvise(ptr, len, MADV_DONTNEED);
#elif defined(_WIN32)
        ::DiscardVirtualMemory(ptr, len);
#endif
    }

    template <typename T> static auto vmDestroy(T* ptr, usize len) -> void {
#if defined(__linux__)
        ::munmap(ptr, len * sizeof(T));
#elif defined(_WIN32)
        ::VirtualFree(ptr, 0, MEM_RELEASE);
#endif
    }

    // ---- Allocation Metadata --------------------------------------------------------------------------------------------------------------------------------
    template <typename T> static auto allocUsableSize(T* ptr) -> usize { return ::malloc_usable_size(static_cast<void*>(ptr)) / sizeof(T); }

    // ---- Freeing --------------------------------------------------------------------------------------------------------------------------------------------
    template <typename T> static auto free(T* ptr) -> void { ::free(static_cast<void*>(ptr)); }
    template <typename T> static auto freeAligned(T* ptr) -> void { ::free(static_cast<void*>(ptr)); }
};