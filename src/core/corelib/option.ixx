export module projnekomata.corelib:option;
import std;
import fmt;
import :invoke_traits;
import :niche;
import :panic;
import :primitives;

struct NoneT_Meta {};

template <typename T> struct SomeT { T value; };
export template <typename T> constexpr auto Some(T&& value) -> SomeT<std::decay_t<T>> { return SomeT<std::decay_t<T>>{ std::forward<T>(value) }; }

export template <typename T> class Option {
public:
    ~Option() { reset(); }

    constexpr Option(NoneT_Meta) : Option() {}
    constexpr Option(SomeT<T>&& other) : Option(std::move(other.value)) {}

    static constexpr bool kNeedsDestruction = !std::is_trivially_destructible_v<T>;
    static constexpr bool kIsCopyable = std::is_copy_constructible_v<T>;
    static constexpr bool kIsMovable = std::is_move_constructible_v<T>;

    constexpr static auto None() -> Option<T> { return Option<T>(); }
    constexpr static auto Some(const T& value) -> Option<T> { return Option<T>(value); }
    constexpr static auto Some(T&& value) -> Option<T> { return Option<T>(std::forward<T>(value)); }

    template <typename F> constexpr static auto someIf(bool cond, F&& f) -> Option<T>
        requires TypedInvocable<F, T>
    {
        if (cond) return Some(f());
        return None();
    }

    constexpr Option(const Option& other) requires kIsCopyable {
        if (other.isSome()) {
            storeObj(*other.ptr());
        } else {
            storeNone();
        }
    }
    constexpr Option& operator=(const Option& other) requires kIsCopyable {
        if (this == &other) return *this;
        reset();
        if (other.isSome()) {
            storeObj(*other.ptr());
        } else {
            storeNone();
        }
        return *this;
    }
    constexpr Option(Option&& other) requires kIsMovable {
        if (other.isSome()) {
            storeObj(std::move(*other.ptr()));
            if constexpr (kNeedsDestruction) other.ptr()->~T();
            other.storeNone();
        } else {
            storeNone();
        }
    }
    constexpr Option& operator=(Option&& other) requires kIsMovable {
        if (this == &other) return *this;
        reset();
        if (other.isSome()) {
            storeObj(std::move(*other.ptr()));
            if constexpr (kNeedsDestruction) other.ptr()->~T();
            other.storeNone();
        }
        return *this;
    }

    constexpr Option(const Option&) requires (!kIsCopyable) = delete;
    constexpr Option& operator=(const Option&) requires (!kIsCopyable) = delete;
    constexpr Option(Option&&) requires (!kIsMovable) = delete;
    constexpr Option& operator=(Option&&) requires (!kIsMovable) = delete;

    // ---- Metadata and Access --------------------------------------------------------------------------------------------------------------------------------

    constexpr auto isSome() const -> bool {
        if constexpr (HasNiche<T>) return !NicheValue<T>::matchesNiche(m_storage);
        else return m_isSome;
    }
    constexpr auto isNone() const -> bool { return !isSome(); }
    constexpr explicit operator bool() const { return isSome(); }

    template <typename P> requires TypedInvocable<P, bool, const T&>
    constexpr auto isSomeAnd(P&& pred) const -> bool { return isSome() && pred(*ptr()); }

    constexpr auto unwrap() const& -> const T& {
        if (isNone()) panic("called `Option::unwrap()` on a None value");
        return *ptr();
    }
    constexpr auto unwrap() & -> T& {
        if (isNone()) panic("called `Option::unwrap()` on a None value");
        return *ptr();
    }
    constexpr auto unwrap() && -> T {
        if (isNone()) panic("called `Option::unwrap()` on a None value");
        return std::move(*ptr());
    }

    constexpr auto unwrapOr(const T& other) const -> const T& {
        if (isNone()) return other;
        return *ptr();
    }
    constexpr auto unwrapOr(T&& other) -> T {
        if (isNone()) return other;
        return *ptr();
    }

    constexpr auto reset() -> void {
        if (isSome()) {
            if constexpr (kNeedsDestruction) ptr()->~T();
            storeNone();
        }
    }

    // ---- Inspection -----------------------------------------------------------------------------------------------------------------------------------------

    template <typename F> requires TypedInvocableNoRet<F, const T&>
    constexpr auto inspect(F&& f) const& -> Option<T> {
        if (isSome()) f(*ptr());
        return *this;
    }

    template <typename F> requires TypedInvocableNoRet<F, const T&>
    constexpr auto inspect(F&& f) && -> Option<T> {
        if (isSome()) f(*ptr());
        return std::move(*this);
    }

    // ---- Functional Programming Bits ------------------------------------------------------------------------------------------------------------------------

    template <typename F> requires TypedInvocableNoRet<F, T&&>
    constexpr auto map(F&& f) const& -> Option<std::invoke_result_t<F, T&&>> {
        using ReturnOptionType = Option<std::invoke_result_t<F, T&&>>;
        if (isNone()) return ReturnOptionType::None();
        return ReturnOptionType::Some(f(std::move(*ptr())));
    }
    template <typename F> requires TypedInvocableNoRet<F, T&&>
    constexpr auto map(F&& f) && -> Option<std::invoke_result_t<F, T&&>> {
        using ReturnOptionType = Option<std::invoke_result_t<F, T&&>>;
        if (isNone()) return ReturnOptionType::None();
        return ReturnOptionType::Some(f(std::move(*ptr())));
    }

private:
    constexpr Option() { storeNone(); }
    constexpr Option(const T& value) { storeObj(value); }
    constexpr Option(T&& value) { storeObj(std::forward<T>(value)); }

    constexpr auto ptr() -> T* { return reinterpret_cast<T*>(&m_storage); }
    constexpr auto ptr() const -> const T* { return reinterpret_cast<const T*>(&m_storage); }

    constexpr auto storeNone() {
        if constexpr (HasNiche<T>) NicheValue<T>::setNiche(m_storage);
        else m_isSome = false;
    }

    constexpr auto storeObj(auto&&... args) {
        new (ptr()) T(std::forward<decltype(args)>(args)...);
        if constexpr (!HasNiche<T>) m_isSome = true;
    }

    using IsSome = std::conditional_t<HasNiche<T>, std::monostate, bool>;

    alignas(T) u8 m_storage[sizeof(T)];
    [[no_unique_address]] IsSome m_isSome;
};

template <typename T> struct fmt::formatter<Option<T>, char, std::enable_if_t<fmt::is_formattable<T>::value>> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    auto format(const Option<T>& value, fmt::format_context& ctx) const -> decltype(ctx.out()) {
        if (value.isSome()) {
            return fmt::format_to(ctx.out(), "Some({})", value.unwrap());
        } else {
            return fmt::format_to(ctx.out(), "None");
        }
    }

};

struct NoneT {
    template<typename T>
    constexpr operator Option<T>() const noexcept {
        return Option<T>(NoneT_Meta{});
    }
};

export inline constexpr auto None = NoneT{};
