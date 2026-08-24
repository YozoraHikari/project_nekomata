module;
#include <cstddef>
export module projnekomata.corelib:iterators;
import std;
import :invoke_traits;
import :niche;
import :nonnull_ptr;
import :option;
import :primitives;

// ---- Helper Concepts ----------------------------------------------------------------------------------------------------------------------------------------

template <typename> struct IsOption : std::false_type {};
template <typename T> struct IsOption<Option<T>> : std::true_type {};
template <typename T> concept OptionType = IsOption<std::remove_cvref_t<T>>::value;

template <typename T> concept Ord = requires(T t) {
    { t <  t } -> std::convertible_to<bool>;
};

template <typename T, typename Other, typename Output> concept Add = requires(T t, Other other, Output output) {
    { t + other } -> std::convertible_to<Output>;
};

template <typename T, typename Other, typename Output> concept Prod = requires(T t, Other other, Output output) {
    { t * other } -> std::convertible_to<Output>;
};

// ---- Iterator Trait  ----------------------------------------------------------------------------------------------------------------------------------------

export template <typename T> concept Iterator = requires(T t) {
    typename T::Item;
    { t.next() } -> std::same_as<Option<typename T::Item>>;
};

// ---- Iterator Builtins --------------------------------------------------------------------------------------------------------------------------------------

export template <typename T> struct IteratorInternalPtr {
    T* ptr;
    explicit constexpr IteratorInternalPtr(T* ptr) : ptr(ptr) {}

    constexpr auto get()        const noexcept -> T* { return ptr; }
    constexpr auto operator*()  const noexcept -> T& { return *ptr; }
    constexpr auto operator->() const noexcept -> T* { return ptr; }

    constexpr auto operator==(const IteratorInternalPtr& other) const noexcept -> bool { return ptr == other.ptr; }
    constexpr auto operator!=(const IteratorInternalPtr& other) const noexcept -> bool { return ptr != other.ptr; }
    constexpr auto operator< (const IteratorInternalPtr& other) const noexcept -> bool { return ptr <  other.ptr; }
};

export template <typename T> class IteratorInternalNonNullPtr {
public:
    NonNullPtr<T> ptr;
    explicit constexpr IteratorInternalNonNullPtr(NonNullPtr<T> ptr) : ptr(ptr) {}

    constexpr auto get()        const noexcept -> T* { return ptr; }
    constexpr auto operator*()  const noexcept -> T& { return *ptr; }
    constexpr auto operator->() const noexcept -> T* { return ptr; }

    constexpr auto operator==(const IteratorInternalNonNullPtr& other) const noexcept -> bool { return ptr == other.ptr; }
    constexpr auto operator!=(const IteratorInternalNonNullPtr& other) const noexcept -> bool { return ptr != other.ptr; }
    constexpr auto operator< (const IteratorInternalNonNullPtr& other) const noexcept -> bool { return ptr <  other.ptr; }

private:
    friend struct NicheValue<IteratorInternalNonNullPtr<T>>;
};


template <typename T> struct NicheValue<IteratorInternalNonNullPtr<T>> {
    static auto setNiche(u8* storage) { NicheValue<NonNullPtr<T>>::setNiche(storage); }
    static bool matchesNiche(const u8* storage) { return NicheValue<NonNullPtr<T>>::matchesNiche(storage); }
};

template <typename T> struct Enumerand {
    usize index; T value;
};

template <typename T> requires HasNiche<T> struct NicheValue<Enumerand<T>> {
    static auto setNiche(u8* storage) { NicheValue<T>::setNiche(storage + offsetof(Enumerand<T>, value)); }
    static bool matchesNiche(const u8* storage) { return NicheValue<T>::matchesNiche(storage + offsetof(Enumerand<T>, value)); }
};

export template <typename K, typename V> struct KeyValue {
    K key; V value;
};

template <typename K, typename V> requires HasNiche<K> struct NicheValue<KeyValue<K, V>> {
    using KV = KeyValue<K, V>;
    static auto setNiche(u8* storage) { NicheValue<K>::setNiche(storage + offsetof(KV, key)); }
    static bool matchesNiche(const u8* storage) { return NicheValue<K>::matchesNiche(storage + offsetof(KV, key)); }
};

template <typename K, typename V> requires (!HasNiche<K> && HasNiche<V>) struct NicheValue<KeyValue<K, V>> {
    using KV = KeyValue<K, V>;
    static auto setNiche(u8* storage) { NicheValue<V>::setNiche(storage + offsetof(KV, value)); }
    static bool matchesNiche(const u8* storage) { return NicheValue<V>::matchesNiche(storage + offsetof(KV, value)); }
};

// ---- Reference Resolve --------------------------------------------------------------------------------------------------------------------------------------

export template <typename T> struct IteratorElementResolveBehavior;
template <typename T> struct IteratorElementResolveBehavior {
    using IntoLambdaByRef  = T&;
    using IntoLambdaByMove = T&&;
    using OwnedType        = std::remove_cvref_t<T>;

    constexpr static auto intoLambdaByRef(T& x) -> IntoLambdaByRef { return x; }
    constexpr static auto intoLambdaByMove(T&& x) -> IntoLambdaByMove { return std::forward<T>(x); }
    constexpr static auto ownedType(T&& x) -> OwnedType { return std::forward<T>(x); }
};
template <typename T> struct IteratorElementResolveBehavior<IteratorInternalPtr<T>> {
    using IntoLambdaByRef =  T&;
    using IntoLambdaByMove = T&;
    using OwnedType        = std::remove_cvref_t<T>;

    constexpr static auto intoLambdaByRef(IteratorInternalPtr<T>& x) -> IntoLambdaByRef { return *x; }
    constexpr static auto intoLambdaByMove(IteratorInternalPtr<T>&& x) -> IntoLambdaByMove { return *x; }
    constexpr static auto ownedType(IteratorInternalPtr<T>&& x) -> OwnedType { return *x; }
};
template <typename T> struct IteratorElementResolveBehavior<IteratorInternalNonNullPtr<T>> {
    using IntoLambdaByRef =  T&;
    using IntoLambdaByMove = T&;
    using OwnedType        = std::remove_cvref_t<T>;

    constexpr static auto intoLambdaByRef(IteratorInternalNonNullPtr<T>& x) -> IntoLambdaByRef { return *x; }
    constexpr static auto intoLambdaByMove(IteratorInternalNonNullPtr<T>&& x) -> IntoLambdaByMove { return *x; }
    constexpr static auto ownedType(IteratorInternalNonNullPtr<T>&& x) -> OwnedType { return *x; }
};
template <typename T> struct IteratorElementResolveBehavior<Enumerand<T>> {
    using Inner = IteratorElementResolveBehavior<T>;
    using IntoLambdaByRef  = Enumerand<typename Inner::IntoLambdaByRef>;
    using IntoLambdaByMove = Enumerand<typename Inner::IntoLambdaByMove>;
    using OwnedType        = Enumerand<typename Inner::OwnedType>;

    constexpr static auto intoLambdaByRef(Enumerand<T>& x) -> IntoLambdaByRef { return { x.index, Inner::intoLambdaByRef(x.value) }; }
    constexpr static auto intoLambdaByMove(Enumerand<T>&& x) -> IntoLambdaByMove { return { x.index, Inner::intoLambdaByMove(std::move(x.value)) }; }
    constexpr static auto ownedType(Enumerand<T>&& x) -> OwnedType { return { x.index, Inner::ownedType(std::move(x.value)) }; }
};
template <typename K, typename V> struct IteratorElementResolveBehavior<KeyValue<K, V>> {
    using InnerK = IteratorElementResolveBehavior<K>;
    using InnerV = IteratorElementResolveBehavior<V>;
    using IntoLambdaByRef  = KeyValue<typename InnerK::IntoLambdaByRef, typename InnerV::IntoLambdaByRef>;
    using IntoLambdaByMove = KeyValue<typename InnerK::IntoLambdaByMove, typename InnerV::IntoLambdaByMove>;
    using OwnedType        = KeyValue<typename InnerK::OwnedType, typename InnerV::OwnedType>;

    constexpr static auto intoLambdaByRef(KeyValue<K, V>& x) -> IntoLambdaByRef { return { InnerK::intoLambdaByRef(x.key), InnerV::intoLambdaByRef(x.value) }; }
    constexpr static auto intoLambdaByMove(KeyValue<K, V>&& x) -> IntoLambdaByMove { return { InnerK::intoLambdaByMove(std::move(x.key)), InnerV::intoLambdaByMove(std::move(x.value)) }; }
    constexpr static auto ownedType(KeyValue<K, V>&& x) -> OwnedType { return { InnerK::ownedType(std::move(x.key)), InnerV::ownedType(std::move(x.value)) }; }
};
template <typename T> using IntoLambdaByRefT = typename IteratorElementResolveBehavior<T>::IntoLambdaByRef;
template <typename T> using IntoLambdaByMoveT = typename IteratorElementResolveBehavior<T>::IntoLambdaByMove;
template <typename T> using OwnedTypeT = typename IteratorElementResolveBehavior<T>::OwnedType;

template <typename T> constexpr auto intoLambdaByRef(T& x) -> IntoLambdaByRefT<T> { return IteratorElementResolveBehavior<T>::intoLambdaByRef(x); }
template <typename T> constexpr auto intoLambdaByMove(T&& x) -> IntoLambdaByMoveT<T> { return IteratorElementResolveBehavior<T>::intoLambdaByMove(std::forward<T>(x)); }
template <typename T> constexpr auto ownedType(T&& x) -> OwnedTypeT<T> { return IteratorElementResolveBehavior<T>::ownedType(std::forward<T>(x)); }

// ---- Pointer/Optional Resolve -------------------------------------------------------------------------------------------------------------------------------

export template <typename T> struct IteratorInternalDeref { using type = T; };
template <typename T> struct IteratorInternalDeref<IteratorInternalPtr<T>> { using type = T; };
template <typename T> struct IteratorInternalDeref<IteratorInternalNonNullPtr<T>> { using type = T; };
template <typename T> using IteratorInternalDerefT = typename IteratorInternalDeref<T>::type;

template <typename T> struct UnwrapOption;
template <typename T> struct UnwrapOption<Option<T>> { using type = T; };
template <typename T> using UnwrapOptionT = typename UnwrapOption<T>::type;

// ---- Basic Iterators ----------------------------------------------------------------------------------------------------------------------------------------

export template <typename Derived> class IteratorBase;

template <Iterator Inner, typename F> class MapIter : public IteratorBase<MapIter<Inner, F>> {
public:
    using Item = std::invoke_result_t<F, IntoLambdaByMoveT<typename Inner::Item>>;

    constexpr MapIter(Inner inner, F f) : m_inner(std::move(inner)), m_f(std::move(f)) {}
    constexpr auto next() -> Option<Item> {
        if (auto next = m_inner.next()) return Some(m_f(intoLambdaByMove(std::move(next.unwrap()))));
        return None;
    }

private:
    Inner m_inner;
    F     m_f;
};

template <Iterator Inner, typename P> class FilterIter : public IteratorBase<FilterIter<Inner, P>> {
public:
    using Item = typename Inner::Item;

    constexpr FilterIter(Inner inner, P p) : m_inner(std::move(inner)), m_p(std::move(p)) {}
    constexpr auto next() -> Option<Item> {
        while (auto next = m_inner.next()) { if (m_p(intoLambdaByRef(next.unwrap()))) return Some(std::move(next.unwrap())); }
        return None;
    }

private:
    Inner m_inner;
    P     m_p;
};

template <Iterator Inner, typename P> class FilterMapIter : public IteratorBase<FilterMapIter<Inner, P>> {
public:
    using Item = UnwrapOptionT<std::invoke_result_t<P, IntoLambdaByMoveT<typename Inner::Item>>>;

    constexpr FilterMapIter(Inner inner, P p) : m_inner(std::move(inner)), m_p(std::move(p)) {}
    constexpr auto next() -> Option<Item> {
        while (auto next = m_inner.next()) { if (auto mapped = m_p(intoLambdaByMove(std::move(next.unwrap())))) return Some(std::move(mapped.unwrap())); }
        return None;
    }

private:
    Inner m_inner;
    P     m_p;
};

template <Iterator Inner> class EnumerateIter : public IteratorBase<EnumerateIter<Inner>> {
public:
    using Item = Enumerand<typename Inner::Item>;

    constexpr EnumerateIter(Inner inner) : m_inner(std::move(inner)) {}
    constexpr auto next() -> Option<Item> {
        if (auto next = m_inner.next()) return Some(Enumerand{m_index++, std::forward<typename Inner::Item>(next.unwrap())});
        return None;
    }

private:
    Inner m_inner;
    usize m_index = 0;
};

template <Iterator I1, Iterator I2> class ZipIter : public IteratorBase<ZipIter<I1, I2>> {
public:
    using Item = KeyValue<typename I1::Item, typename I2::Item>;

    constexpr ZipIter(I1 i1, I2 i2) : m_i1(std::move(i1)), m_i2(std::move(i2)) {}
    constexpr auto next() -> Option<Item> {
        auto next1 = m_i1.next();
        auto next2 = m_i2.next();
        if (next1.isSome() && next2.isSome()) return Some(KeyValue(std::move(next1.unwrap()), std::move(next2.unwrap())));
        return None;
    }

private:
    I1 m_i1;
    I2 m_i2;
};

// ---- Inspector Iterators ------------------------------------------------------------------------------------------------------------------------------------

template <Iterator Inner, typename F> class InspectIter : public IteratorBase<InspectIter<Inner, F>> {
public:
    using Item = typename Inner::Item;

    constexpr InspectIter(Inner inner, F f) : m_inner(std::move(inner)), m_f(std::move(f)) {}
    constexpr auto next() -> Option<Item> {
        if (auto next = m_inner.next()) {
            m_f(intoLambdaByRef(next.unwrap()));
            return Some(std::move(next.unwrap()));
        }
        return None;
    }

private:
    Inner m_inner;
    F     m_f;
};

// ---- CXX iterator shi ---------------------------------------------------------------------------------------------------------------------------------------

template <Iterator Iter> class CxxIterInner {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type        = std::remove_reference_t<typename Iter::Item>;
    using difference_type   = std::ptrdiff_t;
    using pointer           = value_type*;
    using reference         = IntoLambdaByRefT<value_type>;

    class EndSentinel {};

    explicit constexpr CxxIterInner(Iter inner) : m_inner(std::move(inner)) { ++*this; }

    constexpr auto operator++() -> CxxIterInner& { m_current = m_inner.next(); return *this; }
    constexpr auto operator*() -> reference { return intoLambdaByRef(m_current.unwrap()); }
    constexpr auto operator->() -> pointer { return &m_current.unwrap(); }
    constexpr auto operator->() const -> pointer { return &m_current.unwrap(); }
    constexpr auto operator!=(const EndSentinel&) const -> bool { return m_current.isSome(); }

private:
    Iter m_inner;
    Option<typename Iter::Item> m_current = None;
};

// ---- Common CRTP iterator mixin -----------------------------------------------------------------------------------------------------------------------------

export template <typename Derived> class IteratorBase {
public:

    // ---- Iterator Extensions --------------------------------------------------------------------------------------------------------------------------------

    template <typename F> constexpr auto map(F&& f) && requires TypedInvocableNoRet<F, IntoLambdaByMoveT<typename Derived::Item>>
        { return MapIter<Derived, F>(std::move(self()), std::forward<F>(f)); }

    template <typename P> constexpr auto filter(P&& p) &&requires TypedInvocable<P, bool, IntoLambdaByRefT<typename Derived::Item>>
        { return FilterIter<Derived, P>(std::move(self()), std::forward<P>(p)); }

    template <typename P> constexpr auto filterMap(P&& p) &&
        requires TypedInvocableNoRet<P, IntoLambdaByMoveT<typename Derived::Item>> && OptionType<std::invoke_result_t<P, IntoLambdaByMoveT<typename Derived::Item>>>
        { return FilterMapIter<Derived, P>(std::move(self()), std::forward<P>(p)); }

    constexpr auto enumerate() && { return EnumerateIter<Derived>(std::move(self())); }

    template <typename I2> requires Iterator<I2> constexpr auto zip(I2&& i2) && { return ZipIter<Derived, I2>(std::move(self()), std::forward<I2>(i2)); }

    template <typename F> constexpr auto inspect(F&& f) && requires TypedInvocableNoRet<F, IntoLambdaByRefT<typename Derived::Item>>
        { return InspectIter<Derived, F>(std::move(self()), std::forward<F>(f)); }

    // ---- Iterator Reductions --------------------------------------------------------------------------------------------------------------------------------

    constexpr auto sum() requires Add<OwnedTypeT<typename Derived::Item>, IntoLambdaByMoveT<typename Derived::Item>, OwnedTypeT<typename Derived::Item>> {
        using Item = typename Derived::Item;
        using ValueType = OwnedTypeT<Item>;
        ValueType sum = ValueType(0);
        while (auto v = self().next()) sum = std::move(sum) + intoLambdaByMove(std::move(v.unwrap()));
        return sum;
    }

    constexpr auto prod() requires Prod<OwnedTypeT<typename Derived::Item>, IntoLambdaByMoveT<typename Derived::Item>, OwnedTypeT<typename Derived::Item>> {
        using Item = typename Derived::Item;
        using ValueType = OwnedTypeT<Item>;
        ValueType prod = ValueType(1);
        while (auto v = self().next()) prod = std::move(prod) * intoLambdaByMove(std::move(v.unwrap()));
        return prod;
    }

    template <typename P> constexpr auto all(P&& pred) -> bool requires TypedInvocable<P, bool, IntoLambdaByRefT<typename Derived::Item>> {
        while (auto v = self().next()) {
            if (!pred(intoLambdaByRef(v.unwrap()))) return false;
        }
        return true;
    }

    template <typename P> constexpr auto any(P&& pred) -> bool requires TypedInvocable<P, bool, IntoLambdaByRefT<typename Derived::Item>> {
        while (auto v = self().next()) {
            if (pred(intoLambdaByRef(v.unwrap()))) return true;
        }
        return false;
    }

    // TODO: re-NIH a reference type and use that instead of doing the below for pointers

    constexpr auto min() requires Ord<IntoLambdaByRefT<typename Derived::Item>> {
        using Item = typename Derived::Item;
        using ValueType = OwnedTypeT<Item>;

        auto result = Option<ValueType>::None();

        while (auto v = self().next()) {
            if (result.isNone()) { result = Some(ownedType(std::move(v.unwrap()))); continue; }
            if (intoLambdaByRef(v.unwrap()) < intoLambdaByRef(result.unwrap())) result = Some(ownedType(std::move(v.unwrap())));
        }

        return result;
    }


    constexpr auto max() requires Ord<IntoLambdaByRefT<typename Derived::Item>> {
        using Item = typename Derived::Item;
        using ValueType = OwnedTypeT<Item>;

        auto result = Option<ValueType>::None();

        while (auto v = self().next()) {
            if (result.isNone()) { result = Some(ownedType(std::move(v.unwrap()))); continue; }
            if (intoLambdaByRef(v.unwrap()) > intoLambdaByRef(result.unwrap())) result = Some(ownedType(std::move(v.unwrap())));
        }

        return result;
    }

    template <typename F> constexpr auto minByKey(F&& f)
        requires TypedInvocableNoRet<F, IntoLambdaByRefT<typename Derived::Item>>
        && Ord<std::invoke_result_t<F, IntoLambdaByRefT<typename Derived::Item>>>
    {
        using Item = typename Derived::Item;
        using ValueType = OwnedTypeT<Item>;

        auto result = Option<ValueType>::None();

        while (auto v = self().next()) {
            if (result.isNone()) { result = Some(ownedType(std::move(v.unwrap()))); continue; }
            if (f(intoLambdaByRef(v.unwrap())) < f(intoLambdaByRef(result.unwrap()))) result = Some(ownedType(std::move(v.unwrap())));
        }
        return result;
    }

    template <typename F> constexpr auto maxByKey(F&& f)
        requires TypedInvocableNoRet<F, IntoLambdaByRefT<typename Derived::Item>>
        && Ord<std::invoke_result_t<F, IntoLambdaByRefT<typename Derived::Item>>>
    {
        using Item = typename Derived::Item;
        using ValueType = OwnedTypeT<Item>;

        auto result = Option<ValueType>::None();

        while (auto v = self().next()) {
            if (result.isNone()) { result = Some(ownedType(std::move(v.unwrap()))); continue; }
            if (f(intoLambdaByRef(v.unwrap())) > f(intoLambdaByRef(result.unwrap()))) result = Some(ownedType(std::move(v.unwrap())));
        }
        return result;
    }

    // ---- Iterator Find --------------------------------------------------------------------------------------------------------------------------------------

    template <typename P> constexpr auto find(P&& pred) requires TypedInvocable<P, bool, IntoLambdaByRefT<typename Derived::Item>> {
        using Item = typename Derived::Item;
        using ValueType = OwnedTypeT<Item>;

        while (auto v = self().next()) {
            if (pred(intoLambdaByRef(v.unwrap()))) return Option<ValueType>::Some(intoLambdaByMove(std::move(v.unwrap())));
        }
        return Option<ValueType>::None();
    }

    // ---- Collector ------------------------------------------------------------------------------------------------------------------------------------------

    template <template <typename> class Container> constexpr auto collect() {
        using Item = typename Derived::Item;
        using T = OwnedTypeT<Item>;
        auto out = Container<T>::create();
        while (auto v = self().next()) out.emplace(std::move(v.unwrap()));
        return out;
    }

    // ---- CXX iterator shi -----------------------------------------------------------------------------------------------------------------------------------

    constexpr auto begin() const { return CxxIterInner<Derived>(std::move(self())); }
    constexpr auto end() const { return typename CxxIterInner<Derived>::EndSentinel{}; }

private:
    constexpr       Derived& self()       { return static_cast<Derived&>(*this); }
    constexpr const Derived& self() const { return static_cast<const Derived&>(*this); }
};