module;
#if defined(__x86_64__)
#include <immintrin.h>
#elif defined(__aarch64__)
#include <arm_neon.h>
#endif
export module projnekomata.corelib:simd.hal;
import std;
import :primitives;
import :simd.native_reg;

template <usize N> struct ScalarElements;

template <> struct ScalarElements<1> {
    using Uint = u8;
    using Int  = i8;
};

template <> struct ScalarElements<2> {
    using Uint = u16;
    using Int  = i16;
};

template <> struct ScalarElements<4> {
    using Uint = u32;
    using Int  = i32;
};

template <> struct ScalarElements<8> {
    using Uint = u64;
    using Int  = i64;
};

export template <typename T, usize N> struct SimdReg {
    using NativeT = SimdRegNativeT<T, N>;

    SimdReg() = default;
    SimdReg(NativeT data) : m_data(data) {}

    constexpr static auto laneCount() noexcept -> usize { return N; }

    constexpr static auto broadcast(T value) noexcept -> SimdReg<T, N> {
        NativeT data{};
        for (usize i = 0; i < N; i++) data[i] = value;
        return SimdReg(data);
    }

    constexpr static auto fromValues(std::initializer_list<T> list) noexcept -> SimdReg<T, N> {
        NativeT data{};
        for (usize i = 0; i < N; i++) data[i] = list.begin()[i];
        return SimdReg(data);
    }

    constexpr static auto loadUnaligned(const T* ptr) noexcept -> SimdReg<T, N> {
        NativeT data;
        __builtin_memcpy(&data, ptr, sizeof(NativeT));
        return SimdReg(data);
    }

    constexpr static auto loadAligned(const T* ptr) noexcept -> SimdReg<T, N> {
        auto ptra = __builtin_assume_aligned(ptr, alignof(NativeT));

        NativeT data;
        __builtin_memcpy(&data, ptra, sizeof(NativeT));
        return SimdReg(data);
    }

    constexpr auto storeUnaligned(T* ptr) const noexcept -> void {
        __builtin_memcpy(ptr, &m_data, sizeof(NativeT));
    }

    constexpr auto storeAligned(T* ptr) const noexcept -> void {
        auto ptra = __builtin_assume_aligned(ptr, alignof(NativeT));
        __builtin_memcpy(ptra, &m_data, sizeof(NativeT));
    }

    constexpr auto operator[](usize i) const noexcept -> const T& { return m_data[i]; }
    constexpr auto operator[](usize i)       noexcept -> T&       { return m_data[i]; }

    constexpr auto all() const noexcept -> bool {
        return __builtin_reduce_and(m_data != 0);
    }
    constexpr auto any() const noexcept -> bool {
        return __builtin_reduce_or(m_data) != 0;
    }

    constexpr auto cmplaneEq(const SimdReg<T, N>& other) const noexcept -> SimdReg<typename ScalarElements<sizeof(T)>::Int, N> {
        return SimdReg<typename ScalarElements<sizeof(T)>::Int, N>(m_data == other.m_data);
    }
    constexpr auto cmplaneNe(const SimdReg<T, N>& other) const noexcept -> SimdReg<typename ScalarElements<sizeof(T)>::Int, N> {
        return SimdReg<typename ScalarElements<sizeof(T)>::Int, N>(m_data != other.m_data);
    }

    constexpr auto operator==(const SimdReg<T, N>& other) const noexcept -> bool {
        auto cmp = cmplaneEq(other);
        return cmp.all();
    }
    constexpr auto operator!=(const SimdReg<T, N>& other) const noexcept -> bool {
        auto cmp = cmplaneNe(other);
        return cmp.any();
    }

    constexpr auto operator+=(const SimdReg<T, N>& other) noexcept -> SimdReg<T, N>& {
        m_data += other.m_data;
        return *this;
    }
    constexpr friend auto operator+(const SimdReg<T, N>& lhs, const SimdReg<T, N>& rhs) noexcept -> SimdReg<T, N> { return lhs + rhs; }

    constexpr auto operator-=(const SimdReg<T, N>& other) noexcept -> SimdReg<T, N>& {
        m_data -= other.m_data;
        return *this;
    }
    constexpr friend auto operator-(const SimdReg<T, N>& lhs, const SimdReg<T, N>& rhs) noexcept -> SimdReg<T, N> { return lhs - rhs; }

    constexpr auto operator*=(const SimdReg<T, N>& other) noexcept -> SimdReg<T, N>& {
        m_data *= other.m_data;
        return *this;
    }
    constexpr friend auto operator*(const SimdReg<T, N>& lhs, const SimdReg<T, N>& rhs) noexcept -> SimdReg<T, N> { return lhs * rhs; }

    constexpr auto operator/=(const SimdReg<T, N>& other) noexcept -> SimdReg<T, N>& {
        m_data /= other.m_data;
        return *this;
    }
    constexpr friend auto operator/(const SimdReg<T, N>& lhs, const SimdReg<T, N>& rhs) noexcept -> SimdReg<T, N> { return lhs / rhs; }

    constexpr auto operator&=(const SimdReg<T, N>& other) noexcept -> SimdReg<T, N>& {
        m_data &= other.m_data;
        return *this;
    }
    constexpr friend auto operator&(const SimdReg<T, N>& lhs, const SimdReg<T, N>& rhs) noexcept -> SimdReg<T, N> { return lhs & rhs; }

    constexpr auto operator|=(const SimdReg<T, N>& other) noexcept -> SimdReg<T, N>& {
        m_data |= other.m_data;
        return *this;
    }
    constexpr friend auto operator|(const SimdReg<T, N>& lhs, const SimdReg<T, N>& rhs) noexcept -> SimdReg<T, N> { return lhs | rhs; }

    constexpr auto operator^=(const SimdReg<T, N>& other) noexcept -> SimdReg<T, N>& {
        m_data ^= other.m_data;
        return *this;
    }
    constexpr friend auto operator^(const SimdReg<T, N>& lhs, const SimdReg<T, N>& rhs) noexcept -> SimdReg<T, N> { return lhs ^ rhs; }

    constexpr auto operator<<=(usize shift) noexcept -> SimdReg<T, N>& {
        m_data <<= shift;
        return *this;
    }
    constexpr friend auto operator<<(const SimdReg<T, N>& lhs, usize shift) noexcept -> SimdReg<T, N> { return lhs << shift; }

    constexpr auto operator>>=(usize shift) noexcept -> SimdReg<T, N>& {
        m_data >>= shift;
        return *this;
    }
    constexpr friend auto operator>>(const SimdReg<T, N>& lhs, usize shift) noexcept -> SimdReg<T, N> { return lhs >> shift; }

    constexpr auto operator~() const noexcept -> SimdReg<T, N> {
        return SimdReg<T, N>(~m_data);
    }
    constexpr auto operator-() const noexcept -> SimdReg<T, N> {
        return SimdReg<T, N>(-m_data);
    }

    constexpr auto movemask() const noexcept -> ScalarElements<N / 8>::Uint requires (sizeof(T) == 1 && N == 16) {
#if defined(__x86_64__)
        return _mm_movemask_epi8(std::bit_cast<__m128i>(m_data));
#elif defined(__aarch64__)
        u64 b0to7 = vgetq_lane_u64(std::bit_cast<uint64x2_t>(m_data), 0);
        u64 b8to15 = vgetq_lane_u64(std::bit_cast<uint64x2_t>(m_data), 1);

        constexpr u64 magic = 0x000103070f1f3f80ull;
        b0to7 = (b0to7 * magic) >> 56;
        b8to15 = ((b8to15 * magic) >> 48) & 0xff00;
        return b0to7 | b8to15;
#endif
    }

private:
    NativeT m_data;
};
