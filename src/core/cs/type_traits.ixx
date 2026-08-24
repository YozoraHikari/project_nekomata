export module projnekomata.cs:type_traits;
import std;

export template <typename T> struct TTriviallyRelocatable : std::bool_constant<__builtin_is_cpp_trivially_relocatable(T)> {};
template <typename T> inline constexpr bool TTriviallyRelocatableValue = TTriviallyRelocatable<T>::value;

export template <typename T, typename V> struct EnableIfNonVoid {
    using Type = V;
};

template <typename V> struct EnableIfNonVoid<void, V> {
    using Type = void;
};

export template <typename T, typename V> using EnableIfNonVoidT = typename EnableIfNonVoid<T, V>::Type;