export module projnekomata:core.math.matrix_types;
import :core.math.base_matrix;

export namespace projnekomata::math {

template <typename T, usize NDims> using Vector = Matrix<T, 1, NDims>;
using Vector2h   = Matrix<f16, 1, 2>;
using Vector3h   = Matrix<f16, 1, 3>;
using Vector4h   = Matrix<f16, 1, 4>;
using Matrix2x2h = Matrix<f16, 2, 2>;
using Matrix2x3h = Matrix<f16, 3, 2>;
using Matrix2x4h = Matrix<f16, 4, 2>;
using Matrix3x2h = Matrix<f16, 2, 3>;
using Matrix3x3h = Matrix<f16, 3, 3>;
using Matrix3x4h = Matrix<f16, 4, 3>;
using Matrix4x2h = Matrix<f16, 2, 4>;
using Matrix4x3h = Matrix<f16, 3, 4>;
using Matrix4x4h = Matrix<f16, 4, 4>;

using Vector2f   = Matrix<f32, 1, 2>;
using Vector3f   = Matrix<f32, 1, 3>;
using Vector4f   = Matrix<f32, 1, 4>;
using Matrix2x2f = Matrix<f32, 2, 2>;
using Matrix2x3f = Matrix<f32, 3, 2>;
using Matrix2x4f = Matrix<f32, 4, 2>;
using Matrix3x2f = Matrix<f32, 2, 3>;
using Matrix3x3f = Matrix<f32, 3, 3>;
using Matrix3x4f = Matrix<f32, 4, 3>;
using Matrix4x2f = Matrix<f32, 2, 4>;
using Matrix4x3f = Matrix<f32, 3, 4>;
using Matrix4x4f = Matrix<f32, 4, 4>;

using Vector2i   = Matrix<i32, 1, 2>;
using Vector3i   = Matrix<i32, 1, 3>;
using Vector4i   = Matrix<i32, 1, 4>;
using Matrix2x2i = Matrix<i32, 2, 2>;
using Matrix2x3i = Matrix<i32, 3, 2>;
using Matrix2x4i = Matrix<i32, 4, 2>;
using Matrix3x2i = Matrix<i32, 2, 3>;
using Matrix3x3i = Matrix<i32, 3, 3>;
using Matrix3x4i = Matrix<i32, 4, 3>;
using Matrix4x2i = Matrix<i32, 2, 4>;
using Matrix4x3i = Matrix<i32, 3, 4>;
using Matrix4x4i = Matrix<i32, 4, 4>;


}