export module projnekomata.corelib:primitives;
import std;

export using i8  = std::int8_t;
export using i16 = std::int16_t;
export using i32 = std::int32_t;
export using i64 = std::int64_t;

export using u8  = std::uint8_t;
export using u16 = std::uint16_t;
export using u32 = std::uint32_t;
export using u64 = std::uint64_t;

export using usize = std::size_t;

export using f32 = float;
export using f64 = double;
export using f16 = _Float16;

export constexpr u8  operator""_u8 (unsigned long long v) { return static_cast<u8>(v);  }
export constexpr u16 operator""_u16(unsigned long long v) { return static_cast<u16>(v); }
export constexpr u32 operator""_u32(unsigned long long v) { return static_cast<u32>(v); }
export constexpr u64 operator""_u64(unsigned long long v) { return static_cast<u64>(v); }
export constexpr i8  operator""_i8 (unsigned long long v) { return static_cast<i8>(v);  }
export constexpr i16 operator""_i16(unsigned long long v) { return static_cast<i16>(v); }
export constexpr i32 operator""_i32(unsigned long long v) { return static_cast<i32>(v); }
export constexpr i64 operator""_i64(unsigned long long v) { return static_cast<i64>(v); }
export constexpr f16 operator""_f16(long double v) { return static_cast<f16>(v); }
export constexpr f32 operator""_f32(long double v) { return static_cast<f32>(v); }
export constexpr f64 operator""_f64(long double v) { return static_cast<f64>(v); }

export constexpr usize operator""_usize(unsigned long long v) { return static_cast<usize>(v); }