export module projnekomata.corelib:simd;
export import :simd.hal;

using u8x16 = SimdReg<u8, 16>;
using u16x8 = SimdReg<u16, 8>;
using u32x4 = SimdReg<u32, 4>;
using u64x2 = SimdReg<u64, 2>;

using i8x16 = SimdReg<i8, 16>;
using i16x8 = SimdReg<i16, 8>;
using i32x4 = SimdReg<i32, 4>;
using i64x2 = SimdReg<i64, 2>;

using f32x4 = SimdReg<f32, 4>;
using f64x2 = SimdReg<f64, 2>;

using u8x32 = SimdReg<u8, 32>;
using u16x16 = SimdReg<u16, 16>;
using u32x8 = SimdReg<u32, 8>;
using u64x4 = SimdReg<u64, 4>;

using i8x32 = SimdReg<i8, 32>;
using i16x16 = SimdReg<i16, 16>;
using i32x8 = SimdReg<i32, 8>;
using i64x4 = SimdReg<i64, 4>;

using f32x8 = SimdReg<f32, 8>;
using f64x4 = SimdReg<f64, 4>;