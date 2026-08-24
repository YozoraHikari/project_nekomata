export module projnekomata.corelib:flatvariant;
import std;
import :primitives;
import :log;

export template <typename... Ts> class FlatVariant {
public:
    static constexpr usize kSize = std::max({sizeof(Ts)...});
    static constexpr usize kAlignment = std::max({alignof(Ts)...});

    template <typename T> constexpr FlatVariant(T value) requires (std::same_as<T, Ts> || ...)
        : m_index(indexOfT<std::decay_t<T>>())
    {
        new (m_storage) std::decay_t<T>(std::move(value));
    }
    constexpr ~FlatVariant() { destroyInStorage(std::index_sequence_for<Ts...>{}); }

    constexpr FlatVariant(const FlatVariant& other) : m_index(other.m_index) { copyConstructInStorage(other, std::index_sequence_for<Ts...>{}); }
    constexpr FlatVariant(FlatVariant&& other) : m_index(other.m_index) { moveConstructInStorage(std::move(other), std::index_sequence_for<Ts...>{}); }

    constexpr auto operator=(const FlatVariant& other) -> FlatVariant& {
        if (this == &other) return *this;
        destroyInStorage(std::index_sequence_for<Ts...>{});
        m_index = other.m_index;
        copyConstructInStorage(other, std::index_sequence_for<Ts...>{});
        return *this;
    }

    constexpr auto operator=(FlatVariant&& other) noexcept -> FlatVariant& {
        if (this == &other) return *this;
        destroyInStorage(std::index_sequence_for<Ts...>{});
        m_index = other.m_index;
        moveConstructInStorage(std::move(other), std::index_sequence_for<Ts...>{});
        return *this;
    }

    // ---- Match ----------------------------------------------------------------------------------------------------------------------------------------------

    template <typename Visitor> constexpr auto match(Visitor&& vis) {
        using ResultType = std::common_type_t<std::invoke_result_t<Visitor&, Ts&>...>;
        return matchInStorage<ResultType>(std::forward<Visitor>(vis), std::index_sequence_for<Ts...>{});
    }

    template <typename Visitor> constexpr auto match(Visitor&& vis) const {
        using ResultType = std::common_type_t<std::invoke_result_t<Visitor&, const Ts&>...>;
        return matchInStorage<ResultType>(std::forward<Visitor>(vis), std::index_sequence_for<Ts...>{});
    }

private:
    template <typename T> static constexpr auto indexOfT() {
        u32 i = 0;
        u32 index = 0;
        auto _ = ((std::is_same_v<T, Ts> ? (index = i, true) : (i++, false)) || ...);
        return index;
    }

    template <usize... Is> constexpr auto destroyInStorage(std::index_sequence<Is...>) -> void {
        auto _ = ((m_index == static_cast<u32>(Is) && (std::destroy_at(reinterpret_cast<Ts*>(m_storage)), true)) || ...);
    }

    template <usize... Is> constexpr auto copyConstructInStorage(const FlatVariant& other, std::index_sequence<Is...>) -> void {
        auto _ = ((m_index == static_cast<u32>(Is) && (new (m_storage) Ts(*reinterpret_cast<const Ts*>(other.m_storage)), true)) || ...);
    }

    template <usize... Is> constexpr auto moveConstructInStorage(FlatVariant&& other, std::index_sequence<Is...>) -> void {
        auto _ = ((m_index == static_cast<u32>(Is) && (new (m_storage) Ts(std::move(*reinterpret_cast<Ts*>(other.m_storage))), true)) || ...);
    }

    template <typename ResultType, typename Visitor, usize... Is> constexpr auto matchInStorage(Visitor&& vis, std::index_sequence<Is...>) -> ResultType {
        using Pfn = ResultType(*)(Visitor&, u8*);
        static constexpr Pfn pfntable[] = {
            [](Visitor& vis, u8* stor) -> ResultType { return vis(*reinterpret_cast<Ts*>(stor)); }...
        };
        return pfntable[m_index](vis, m_storage);
    }

    template <typename ResultType, typename Visitor, usize... Is> constexpr auto matchInStorage(Visitor&& vis, std::index_sequence<Is...>) const -> ResultType {
        using Pfn = ResultType(*)(Visitor&, const u8*);
        static constexpr Pfn pfntable[] = {
            [](Visitor& vis, const u8* stor) -> ResultType { return vis(*reinterpret_cast<const Ts*>(stor)); }...
        };
        return pfntable[m_index](vis, m_storage);
    }

    u32 m_index;
    alignas(kAlignment) u8 m_storage[kSize];

    template <typename T, typename... Us> friend constexpr bool matches(const FlatVariant<Us...>&);
    template <typename T, typename... Us> friend constexpr T& acquireInto(FlatVariant<Us...>&);
    template <typename T, typename... Us> friend constexpr const T& acquireInto(const FlatVariant<Us...>&);
};

export template <typename T, typename... Ts> constexpr bool matches(const FlatVariant<Ts...>& v) {
    return v.m_index == FlatVariant<Ts...>::template indexOfT<std::decay_t<T>>();
}

export template <typename T, typename... Ts> constexpr T& acquireInto(FlatVariant<Ts...>& v) {
    return *reinterpret_cast<T*>(v.m_storage);
}

export template <typename T, typename... Ts> constexpr const T& acquireInto(const FlatVariant<Ts...>& v) {
    return *reinterpret_cast<const T*>(v.m_storage);
}