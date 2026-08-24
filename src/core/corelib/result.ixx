export module projnekomata.corelib:result;
import std;
import fmt;
import :flatvariant;
import :invoke_traits;
import :option;
import :panic;


template <typename T> struct OkT { T value; };
export template <typename T> constexpr auto Ok(T&& value) -> OkT<std::decay_t<T>> { return OkT<std::decay_t<T>>{ std::forward<T>(value) }; }

template <typename T> struct ErrT { T value; };
export template <typename T> constexpr auto Err(T&& value) -> ErrT<std::decay_t<T>> { return ErrT<std::decay_t<T>>{ std::forward<T>(value) }; }

export template <typename T, typename E> class Result {
public:
    constexpr Result(OkT<T>&& other) : m_variant(std::move(other.value)) {}
    constexpr Result(ErrT<E>&& other) : m_variant(std::move(other.value)) {}

    constexpr static auto Ok(const T& value) -> Result<T, E> { return Result<T, E>(value); }
    constexpr static auto Err(const E& error) -> Result<T, E> { return Result<T, E>(error); }
    constexpr static auto Ok(T&& value) -> Result<T, E> { return Result<T, E>(std::move(value)); }
    constexpr static auto Err(E&& error) -> Result<T, E> { return Result<T, E>(std::move(error)); }

    constexpr auto isOk() const -> bool { return matches<T>(m_variant); }
    constexpr auto isErr() const -> bool { return matches<E>(m_variant); }

    template <typename P> requires TypedInvocable<P, bool, const T&>
    constexpr auto isOkAnd(P&& pred) const -> bool { return isOk() && pred(acquireInto<T>(m_variant)); }

    template <typename P> requires TypedInvocable<P, bool, const E&>
    constexpr auto isErrAnd(P&& pred) const -> bool { return isErr() && pred(acquireInto<E>(m_variant)); }

    constexpr auto unwrap() const& -> const T& {
        if (isErr()) panic("called `Result::unwrap()` on an Err value");
        return acquireInto<T>(m_variant);
    }
    constexpr auto unwrap() & -> T& {
        if (isErr()) panic("called `Result::unwrap()` on an Err value");
        return acquireInto<T>(m_variant);
    }
    constexpr auto unwrap() && -> T {
        if (isErr()) panic("called `Result::unwrap()` on an Err value");
        return std::move(acquireInto<T>(m_variant));
    }

    constexpr auto unwrapErr() const& -> const E& {
        if (isOk()) panic("called `Result::unwrapErr()` on an Ok value");
        return acquireInto<E>(m_variant);
    }
    constexpr auto unwrapErr() & -> E& {
        if (isOk()) panic("called `Result::unwrapErr()` on an Ok value");
        return acquireInto<E>(m_variant);
    }
    constexpr auto unwrapErr() && -> E {
        if (isOk()) panic("called `Result::unwrapErr()` on an Ok value");
        return std::move(acquireInto<E>(m_variant));
    }

    constexpr auto ok() -> Option<T> {
        if (isErr()) return None;
        return Some(std::move(acquireInto<T>(m_variant)));
    }
    constexpr auto err() -> Option<E> {
        if (isOk()) return None;
        return Some(std::move(acquireInto<E>(m_variant)));
    }
    
    // ---- Inspection -----------------------------------------------------------------------------------------------------------------------------------------
    
    template <typename F> requires TypedInvocableNoRet<F, const T&>
    constexpr auto inspect(F&& f) const& -> Result<T, E> {
        if (isOk()) f(acquireInto<T>(m_variant));
        return *this;
    }
    template <typename F> requires TypedInvocableNoRet<F, const T&>
    constexpr auto inspect(F&& f) && -> Result<T, E> {
        if (isOk()) f(acquireInto<T>(m_variant));
        return std::move(*this);
    }
    template <typename F> requires TypedInvocableNoRet<F, const E&>
    constexpr auto inspectErr(F&& f) const& -> Result<T, E> {
        if (isErr()) f(acquireInto<E>(m_variant));
        return *this;
    }
    template <typename F> requires TypedInvocableNoRet<F, const E&>
    constexpr auto inspectErr(F&& f) && -> Result<T, E> {
        if (isErr()) f(acquireInto<E>(m_variant));
        return std::move(*this);
    }

private:
    constexpr explicit Result(const T& value) : m_variant(value) {}
    constexpr explicit Result(const E& error) : m_variant(error) {}
    constexpr explicit Result(T&& value) : m_variant(std::move(value)) {}
    constexpr explicit Result(E&& error) : m_variant(std::move(error)) {}

    FlatVariant<T, E> m_variant;
};

template <typename T, typename E> struct fmt::formatter<Result<T, E>, char, std::enable_if_t<fmt::is_formattable<T>::value && fmt::is_formattable<E>::value>> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    auto format(const Result<T, E>& value, fmt::format_context& ctx) const -> decltype(ctx.out()) {
        if (value.isOk()) {
            return fmt::format_to(ctx.out(), "Ok({})", value.unwrap());
        } else {
            return fmt::format_to(ctx.out(), "Err({})", value.unwrapErr());
        }
    }
};