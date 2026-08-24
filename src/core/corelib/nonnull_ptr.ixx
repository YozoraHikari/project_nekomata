export module projnekomata.corelib:nonnull_ptr;
import :niche;
import :option;

export template <typename T> class NonNullPtr {
public:
    NonNullPtr() = delete;
    explicit NonNullPtr(T* ptr) {
        if (ptr == nullptr) __builtin_trap();
        m_ptr = ptr;
    }

    NonNullPtr(const NonNullPtr&) = default;
    NonNullPtr& operator=(const NonNullPtr&) = default;
    NonNullPtr(NonNullPtr&&) = default;
    NonNullPtr& operator=(NonNullPtr&&) = default;

    constexpr auto get()        const noexcept -> T* { return m_ptr; }
    constexpr auto operator*()  const noexcept -> T& { return *m_ptr; }
    constexpr auto operator->() const noexcept -> T* { return m_ptr; }

    constexpr auto operator==(const NonNullPtr& other) const noexcept -> bool { return m_ptr == other.m_ptr; }
    constexpr auto operator!=(const NonNullPtr& other) const noexcept -> bool { return m_ptr != other.m_ptr; }
    constexpr auto operator< (const NonNullPtr& other) const noexcept -> bool { return m_ptr <  other.m_ptr; }

private:
    T* m_ptr;

    friend struct NicheValue<NonNullPtr<T>>;
    explicit constexpr NonNullPtr(std::nullptr_t) : m_ptr(nullptr) {}
};

template <typename T> struct NicheValue<NonNullPtr<T>> {
    static auto setNiche(u8* storage) { std::memset(storage, 0, sizeof(NonNullPtr<T>)); }
    static bool matchesNiche(const u8* storage) { u8* nullptrBits = nullptr; return std::memcmp(storage, &nullptrBits, sizeof(NonNullPtr<T>)) == 0; }
};
static_assert(sizeof(Option<NonNullPtr<u32>>) == sizeof(u32*), "NonNullPtr should have null pointer optimization");