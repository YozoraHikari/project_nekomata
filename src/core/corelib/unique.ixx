export module projnekomata.corelib:unique;
import std;
import :cmp_traits;

export template <typename T> class Unique {
public:
    constexpr Unique(std::nullptr_t) noexcept : m_ptr(nullptr) {}
    ~Unique() { delete m_ptr; }

    constexpr static auto null() noexcept -> Unique { return Unique(nullptr); }

    template <typename... Args>
    constexpr static auto create(Args&&... args) noexcept -> Unique {
        auto uniq = Unique::null();
        uniq.m_ptr = new T(std::forward<Args>(args)...);
        return uniq;
    }

    constexpr static auto from(T&& t) noexcept -> Unique { return Unique(new T(std::forward<T>(t))); }

    template <typename D> constexpr static auto upcast(Unique<D>&& other) noexcept -> Unique<T> {
        static_assert(std::is_base_of_v<T, D>, "D must be a base of T");
        static_assert(std::has_virtual_destructor_v<T>, "T must have a virtual destructor");
        auto ptr = null();
        ptr.m_ptr = static_cast<T*>(other.leak());
        return ptr;
    }

    constexpr Unique(const Unique& other) = delete;
    constexpr Unique(Unique&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }

    constexpr auto operator=(const Unique& other) = delete;
    constexpr auto operator=(Unique&& other) noexcept -> Unique& {
        delete m_ptr;
        m_ptr = other.leak();
        return *this;
    }

    constexpr auto operator->() const noexcept -> T* { return m_ptr; }
    constexpr auto operator*() const noexcept -> T& { return *m_ptr; }

    constexpr auto ptr() const noexcept -> T* { return m_ptr; }
    constexpr auto isNull() const noexcept -> bool { return m_ptr == nullptr; }

    constexpr auto release() noexcept -> void {
        delete m_ptr;
        m_ptr = nullptr;
    }
    constexpr auto leak() noexcept -> T* {
        auto ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    constexpr auto operator==(const Unique& other) const noexcept -> bool {
        if (isNull() && other.isNull()) return true;
        if (isNull() || other.isNull()) return false;
        return *m_ptr == *other.m_ptr;
    }
    constexpr auto operator!=(const Unique& other) const noexcept -> bool { return !(*this == other); }
    constexpr auto operator< (const Unique& other) const noexcept -> bool { return *m_ptr < *other.m_ptr; }
    constexpr auto operator> (const Unique& other) const noexcept -> bool { return *other < *this; }

private:
    constexpr explicit Unique(T* ptr) noexcept : m_ptr(ptr) {}

    T* m_ptr;
};