export module projnekomata:core.math.base_matrix;
import std;
import projnekomata.corelib;
import :core.math.consts;

static_assert(__has_extension(matrix_types_scalar_division));

export namespace projnekomata::math {

template <typename T, usize R, usize C> class Matrix {
public:
    Matrix() = default;
    Matrix(std::initializer_list<T> list) {
        debug_assert(list.size() == R * C, "The initializer list must have the same size as the matrix.");

        auto it = list.begin();
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                mself(r, c) = *it++;
    }
    explicit Matrix(T value) {
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                mself(r, c) = value;
    }

    template <typename... Args>
        requires (sizeof...(Args) == R * C)
              && (std::is_convertible_v<Args, T> && ...)
    explicit Matrix(Args&&... args) {
        T flat[] = { static_cast<T>(std::forward<Args>(args))... };
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                mself(r, c) = flat[r * C + c];
    }

    constexpr static auto fill(T value) -> Matrix {
        Matrix result{};
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                result[r, c] = value;
        return result;
    }

    constexpr static auto identity() -> Matrix requires (R == C) {
        Matrix result{};
        for (usize i = 0; i < R; i++)
            result[i, i] = T(1);
        return result;
    }

    constexpr static auto zero() -> Matrix {
        return fill(T(0));
    }

    constexpr static auto one() -> Matrix {
        return fill(T(1));
    }

    constexpr auto operator[](usize r, usize c) const -> const T& { return mself(r, c); }
    constexpr auto operator[](usize r, usize c)       ->       T& { return mself(r, c); }


    constexpr auto operator+=(const Matrix& other) -> Matrix& { storeLlvmMatrix(loadLlvmMatrix() + other.loadLlvmMatrix()); return *this; }
    constexpr auto operator-=(const Matrix& other) -> Matrix& { storeLlvmMatrix(loadLlvmMatrix() - other.loadLlvmMatrix()); return *this; }
    constexpr auto operator*=(const Matrix& other) -> Matrix& { storeLlvmMatrix(loadLlvmMatrix() * other.loadLlvmMatrix()); return *this; }
    constexpr auto operator+=(T scalar) -> Matrix& { storeLlvmMatrix(loadLlvmMatrix() + scalar); return *this; }
    constexpr auto operator-=(T scalar) -> Matrix& { storeLlvmMatrix(loadLlvmMatrix() - scalar); return *this; }
    constexpr auto operator*=(T scalar) -> Matrix& { storeLlvmMatrix(loadLlvmMatrix() * scalar); return *this; }
    constexpr auto operator/=(T scalar) -> Matrix& { storeLlvmMatrix(loadLlvmMatrix() / scalar); return *this; }

    constexpr friend auto operator+(Matrix lhs, const Matrix& rhs) -> Matrix { return lhs += rhs; }
    constexpr friend auto operator+(Matrix lhs, const T& rhs) -> Matrix { return lhs += rhs; }
    constexpr friend auto operator+(const T& rhs, Matrix lhs) -> Matrix { return lhs += rhs; }
    constexpr friend auto operator-(Matrix lhs, const Matrix& rhs) -> Matrix { return lhs -= rhs; }
    constexpr friend auto operator-(Matrix lhs, const T& rhs) -> Matrix { return lhs -= rhs; }
    constexpr friend auto operator-(const T& rhs, Matrix lhs) -> Matrix { return lhs -= rhs; }
    constexpr friend auto operator*(Matrix lhs, const T& rhs) -> Matrix { return lhs *= rhs; }
    constexpr friend auto operator*(const T& rhs, Matrix lhs) -> Matrix { return lhs *= rhs; }
    constexpr friend auto operator/(Matrix lhs, const T& rhs) -> Matrix { return lhs /= rhs; }
    constexpr friend auto operator/(const T& rhs, Matrix lhs) -> Matrix { return lhs /= rhs; }

    template <usize Nc>
    constexpr friend auto operator*(Matrix lhs, const Matrix<T, C, Nc>& rhs) -> Matrix<T, R, Nc> {
        return Matrix<T, R, Nc>(lhs.loadLlvmMatrix() * rhs.loadLlvmMatrix());
    }

    constexpr auto operator==(const Matrix& other) const -> bool {
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                if (mself(r, c) != other[r, c]) return false;
        return true;
    }
    constexpr auto operator!=(const Matrix& other) const -> bool { return !(*this == other); }

    constexpr auto componentWiseMultiply(const Matrix& other) const -> Matrix {
        Matrix result{};
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                result[r, c] = mself(r, c) * other[r, c];
        return result;
    }
    constexpr auto componentWiseDivide(const Matrix& other) const -> Matrix {
        Matrix result{};
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                result[r, c] = mself(r, c) / other[r, c];
        return result;
    }

    constexpr auto transpose() const -> Matrix {
        return Matrix(__builtin_matrix_transpose(loadLlvmMatrix()));
    }

    constexpr auto x() const -> const T& { return mself(0, 0); }
    constexpr auto y() const -> const T& { return mself(1, 0); }
    constexpr auto z() const -> const T& { return mself(2, 0); }
    constexpr auto w() const -> const T& { return mself(3, 0); }
    constexpr auto x()       ->       T& { return mself(0, 0); }
    constexpr auto y()       ->       T& { return mself(1, 0); }
    constexpr auto z()       ->       T& { return mself(2, 0); }
    constexpr auto w()       ->       T& { return mself(3, 0); }

    constexpr auto sum() const -> T {
        T sum = T(0);
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                sum += mself(r, c);
        return sum;
    }
    constexpr auto prod() const -> T {
        T prod = T(1);
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                prod *= mself(r, c);
        return prod;
    }

    constexpr auto dot(const Matrix& other) const -> T requires (C == 1) {
        return componentWiseMultiply(other).sum();
    }

    constexpr auto cross(const Matrix& other) const -> Matrix<T, 3, 1> requires (C == 1 && R == 3) {
        return Matrix<T, 3, 1>(
            y() * other.z() - z() * other.y(),
            z() * other.x() - x() * other.z(),
            x() * other.y() - y() * other.x()
        );
    }

    constexpr auto lengthSquared() const -> T requires (C == 1) {
        return dot(*this);
    }

    constexpr auto length() const -> T requires (C == 1) {
        return std::sqrt(lengthSquared());
    }

    constexpr auto normalize() const -> Matrix requires (C == 1) {
        return *this / length();
    }

    constexpr auto round() const -> Matrix {
        Matrix result;
        for (usize r = 0; r < R; r++)
            for (usize c = 0; c < C; c++)
                result[r, c] = std::round(mself(r, c));
        return result;
    }

    constexpr auto decomposePosition() const -> Matrix<T, 3, 1> requires (C == 4 && R == 4) {
        return submatrix<3, 1>(0, 3);
    }


    [[nodiscard]] Option<Matrix> inverse() const requires (C == R && R <= 4) {
        if constexpr (R == 1) return invertMatrix1x1(*this);
        if constexpr (R == 2) return invertMatrix2x2(*this);
        if constexpr (R == 3) return invertMatrix3x3(*this);
        if constexpr (R == 4) return invertMatrix4x4(*this);

        // TODO: Add a fallback later. For the moment it's not necessary.
        return None;
    }

    [[nodiscard]] Matrix inverseRigid() const requires (C == 4 && R == 4) {
        Matrix<T, 3, 3> rotMat = submatrix<3, 3>(0, 0);
        Matrix<T, 3, 1> transl = submatrix<3, 1>(0, 3);

        auto rotInverse = rotMat.transpose(); // for rotation matrices transposition is inversion
        auto translInverse = rotInverse * transl * -1.0f;

        return {
            rotInverse[0, 0], rotInverse[0, 1], rotInverse[0, 2], translInverse[0, 0],
            rotInverse[1, 0], rotInverse[1, 1], rotInverse[1, 2], translInverse[1, 0],
            rotInverse[2, 0], rotInverse[2, 1], rotInverse[2, 2], translInverse[2, 0],
            T(0),             T(0),             T(0),             T(1)
        };
    }

    template <usize R2, usize C2> constexpr auto submatrix(usize atRow, usize atCol) const -> Matrix<T, R2, C2> {
        debug_assert(atRow + R2 <= R, "submatrix out of bounds by row address");
        debug_assert(atCol + C2 <= C, "submatrix out of bounds by column address");
        auto ptr = m_data + (atRow + atCol * R);
        auto accelForm = __builtin_matrix_column_major_load(ptr, R2, C2, R);
        return Matrix<T, R2, C2>(accelForm);
    }

private:
    using LlvmMatrixType __attribute__((matrix_type(R, C))) = T;
    using Storage = T[R * C];
    Storage m_data;

    constexpr auto loadLlvmMatrix() const -> LlvmMatrixType {
        return __builtin_matrix_column_major_load(m_data, R, C, R);
    }
    constexpr auto storeLlvmMatrix(LlvmMatrixType data) -> void {
        __builtin_matrix_column_major_store(data, m_data, R);
    }

    Matrix(Storage data) : m_data(data) {}
    Matrix(LlvmMatrixType data) {
        storeLlvmMatrix(data);
    }

    template <typename, usize, usize> friend class Matrix;

    constexpr auto mself(usize r, usize c) const -> const T& { return m_data[c * R + r]; }
    constexpr auto mself(usize r, usize c)       ->       T& { return m_data[c * R + r]; }

    /// Inverts a 1x1 matrix.
    ///
    /// It does so by simply computing the reciprocal of the element.
    static auto invertMatrix1x1(Matrix<T, 1, 1> mat) -> Option<Matrix<T, 1, 1>> {
        T det = mat[0, 0];

        if (std::abs(det) < consts::epsilonValue<T>()) {
            return None;
        }

        Matrix result = { T(1) / det };
        return Some(result);
    }

    /// Inverts a 2x2 matrix.
    ///
    /// Uses the 1/det (ad-bc) formula.
    static auto invertMatrix2x2(Matrix<T, 2, 2> mat) -> Option<Matrix<T, 2, 2>> {
        T a = mat[0, 0];
        T b = mat[0, 1];
        T c = mat[1, 0];
        T d = mat[1, 1];

        T det = a * d - b * c;

        T scale = std::max({ std::abs(a), std::abs(b), std::abs(c), std::abs(d) });
        T threshold = consts::epsilonValue<T>() * scale * scale * scale;

        if (std::abs(det) < threshold) {
            return None;
        }

        T invDet = T(1) / det;

        Matrix result = {
            d * invDet, -b * invDet,
           -c * invDet,  a * invDet
        };
        return Some(result);
    }

    /// Inverts a 3x3 matrix.
    ///
    /// Uses the cofactor and adjugate method.
    static auto invertMatrix3x3(Matrix<T, 3, 3> mat) -> Option<Matrix<T, 3, 3>> {
        T m00 = mat[0, 0], m01 = mat[0, 1], m02 = mat[0, 2];
        T m10 = mat[1, 0], m11 = mat[1, 1], m12 = mat[1, 2];
        T m20 = mat[2, 0], m21 = mat[2, 1], m22 = mat[2, 2];

        T c00 = m11 * m22 - m12 * m21;
        T c10 = -(m10 * m22 - m12 * m20);
        T c20 =  m10 * m21 - m11 * m20;
        T c01 = -(m01 * m22 - m02 * m21);
        T c11 =  m00 * m22 - m02 * m20;
        T c21 = -(m00 * m21 - m01 * m20);
        T c02 =  m01 * m12 - m02 * m11;
        T c12 = -(m00 * m12 - m02 * m10);
        T c22 =  m00 * m11 - m01 * m10;

        T det = m00 * c00 + m01 * c10 + m02 * c20;
        T scale = std::max({
            std::abs(m00), std::abs(m01), std::abs(m02),
            std::abs(m10), std::abs(m11), std::abs(m12),
            std::abs(m20), std::abs(m21), std::abs(m22)
        });
        T threshold = consts::epsilonValue<T>() * scale * scale * scale;

        if (std::abs(det) < threshold) {
            return None;
        }

        T invDet = T(1) / det;

        Matrix result = {
            c00 * invDet, c01 * invDet, c02 * invDet,
            c10 * invDet, c11 * invDet, c12 * invDet,
            c20 * invDet, c21 * invDet, c22 * invDet
        };
        return Some(result);
    }

    /// Inverts a 4x4 matrix.
    ///
    /// Uses a cofactor expansion struck using 2x2 subdeterminants to avoid all 3x3 determinant calculations.
    static auto invertMatrix4x4(Matrix<T, 4, 4> mat) -> Option<Matrix<T, 4, 4>> {
        T m00 = mat[0, 0], m01 = mat[0, 1], m02 = mat[0, 2], m03 = mat[0, 3];
        T m10 = mat[1, 0], m11 = mat[1, 1], m12 = mat[1, 2], m13 = mat[1, 3];
        T m20 = mat[2, 0], m21 = mat[2, 1], m22 = mat[2, 2], m23 = mat[2, 3];
        T m30 = mat[3, 0], m31 = mat[3, 1], m32 = mat[3, 2], m33 = mat[3, 3];

        // Determinants of 2x2 minors from left block
        T s0 = m00 * m11 - m01 * m10;
        T s1 = m00 * m21 - m01 * m20;
        T s2 = m00 * m31 - m01 * m30;
        T s3 = m10 * m21 - m11 * m20;
        T s4 = m10 * m31 - m11 * m30;
        T s5 = m20 * m31 - m21 * m30;

        // Determinants of 2x2 minors from right block
        T c5 = m22 * m33 - m23 * m32;
        T c4 = m12 * m33 - m13 * m32;
        T c3 = m12 * m23 - m13 * m22;
        T c2 = m02 * m33 - m03 * m32;
        T c1 = m02 * m23 - m03 * m22;
        T c0 = m02 * m13 - m03 * m12;

        T det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;

        T scale = std::max({
            std::abs(m00), std::abs(m01), std::abs(m02), std::abs(m03),
            std::abs(m10), std::abs(m11), std::abs(m12), std::abs(m13),
            std::abs(m20), std::abs(m21), std::abs(m22), std::abs(m23),
            std::abs(m30), std::abs(m31), std::abs(m32), std::abs(m33)
        });
        T threshold = consts::epsilonValue<T>() * scale * scale * scale;

        if (std::abs(det) < threshold) {
            return None;
        }

        T invDet = T(1) / det;

        Matrix result = {
            invDet * ( m11 * c5 - m21 * c4 + m31 * c3), invDet * (-m01 * c5 + m21 * c2 - m31 * c1), invDet * ( m01 * c4 - m11 * c2 + m31 * c0), invDet * (-m01 * c3 + m11 * c1 - m21 * c0),
            invDet * (-m10 * c5 + m20 * c4 - m30 * c3), invDet * ( m00 * c5 - m20 * c2 + m30 * c1), invDet * (-m00 * c4 + m10 * c2 - m30 * c0), invDet * ( m00 * c3 - m10 * c1 + m20 * c0),
            invDet * ( m13 * s5 - m23 * s4 + m33 * s3), invDet * (-m03 * s5 + m23 * s2 - m33 * s1), invDet * ( m03 * s4 - m13 * s2 + m33 * s0), invDet * (-m03 * s3 + m13 * s1 - m23 * s0),
            invDet * (-m12 * s5 + m22 * s4 - m32 * s3), invDet * ( m02 * s5 - m22 * s2 + m32 * s1), invDet * (-m02 * s4 + m12 * s2 - m32 * s0), invDet * ( m02 * s3 - m12 * s1 + m22 * s0)
        };
        return Some(result);
    }

};

} // namespace projnekomata::math