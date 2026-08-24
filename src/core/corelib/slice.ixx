export module projnekomata.corelib:slice;
import :cmp_traits;
import :iterators;
import :primitives;

// -------------------------------------------------------------------------------------------------------------------------------------------------------------

template <typename T> class SliceIter : public IteratorBase<SliceIter<T>> {
public:
    using Item = IteratorInternalNonNullPtr<T>;

    constexpr SliceIter(T* begin, T* end) : m_begin(begin), m_end(end) {}
    constexpr auto next() -> Option<Item> {
        if (m_begin == m_end) return None;
        return Some(IteratorInternalNonNullPtr(NonNullPtr<T>(m_begin++)));
    }

private:
    T* m_begin;
    T* m_end;
};

template <typename T> class ReverseSliceIter : public IteratorBase<ReverseSliceIter<T>> {
public:
    using Item = IteratorInternalNonNullPtr<T>;

    constexpr ReverseSliceIter(T* begin, T* end) : m_begin(begin), m_end(end) {}
    constexpr auto next() -> Option<Item> {
        if (m_begin == m_end) return None;
        return Some(IteratorInternalNonNullPtr(NonNullPtr<T>(m_begin--)));
    }

private:
    T* m_begin;
    T* m_end;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------

export template <typename T> class Slice {
public:
    constexpr Slice() noexcept : m_ptr(nullptr), m_len(0) {}
    constexpr Slice(T* ptr, usize len) noexcept : m_ptr(ptr), m_len(len) {}

    constexpr auto data() const noexcept -> T* { return m_ptr; }
    constexpr auto begin() const noexcept -> const T* { return m_ptr; }
    constexpr auto end() const noexcept -> const T* { return m_ptr + m_len; }
    constexpr auto len() const noexcept -> usize { return m_len; }
    constexpr auto size() const noexcept -> usize { return m_len; }

    constexpr T& first() noexcept { return m_ptr[0]; }
    constexpr const T& first() const noexcept { return m_ptr[0]; }
    constexpr T& last() noexcept { return m_ptr[m_len - 1]; }
    constexpr const T& last() const noexcept { return m_ptr[m_len - 1]; }

    constexpr auto operator[](usize index) const noexcept -> T& { return m_ptr[index]; }

    constexpr auto isEmpty() const noexcept -> bool { return m_len == 0; }

    constexpr auto iter() const noexcept -> SliceIter<T> { return SliceIter<T>(m_ptr, m_ptr + m_len); }
    constexpr auto iterRev() const noexcept -> ReverseSliceIter<T> { return ReverseSliceIter<T>(m_ptr + m_len - 1, m_ptr - 1); }

private:
    T* m_ptr;
    usize m_len;
};

export template <typename T> class StaticSlice : public Slice<T> {
public:
    template <T... Values> constexpr static auto inst() noexcept -> Slice<const T> {
        static constexpr T values[] = { Values... };
        return Slice<const T>(static_cast<const T*>(values), sizeof(values) / sizeof(T));
    }
};

export template <typename A, typename B> requires Eq<A, B> auto operator==(const Slice<A>& rhs, const Slice<B>& lhs) noexcept -> bool {
    return lhs.len() == rhs.len() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

export extern template class Slice<u8>;
export extern template class Slice<u16>;
export extern template class Slice<u32>;
export extern template class Slice<u64>;
export extern template class Slice<i8>;
export extern template class Slice<i16>;
export extern template class Slice<i32>;
export extern template class Slice<i64>;
export extern template class Slice<f32>;
export extern template class Slice<f64>;
export extern template class Slice<bool>;

export extern template class Slice<const u8>;
export extern template class Slice<const u16>;
export extern template class Slice<const u32>;
export extern template class Slice<const u64>;
export extern template class Slice<const i8>;
export extern template class Slice<const i16>;
export extern template class Slice<const i32>;
export extern template class Slice<const i64>;
export extern template class Slice<const f32>;
export extern template class Slice<const f64>;
export extern template class Slice<const bool>;