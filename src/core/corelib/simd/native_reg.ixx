export module projnekomata.corelib:simd.native_reg;
import :primitives;

export template <typename T, usize N> using SimdRegNativeT __attribute__((vector_size(N * sizeof(T)))) = T;