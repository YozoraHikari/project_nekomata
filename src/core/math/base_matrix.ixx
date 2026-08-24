export module projnekomata:core.math.base_matrix;
import std;
import projnekomata.cs;
import :core.math.consts;

export namespace projnekomata::math {

template <typename T, usize NRows> class MatrixStorageVector {
public:
    using Type = T[NRows];

    MatrixStorageVector() = default;
    MatrixStorageVector(const MatrixStorageVector&) = default;

    Type m_memory;

    void broadcast(T x) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] = x;
        }
    }

    T& operator[](usize row) {
        return m_memory[row];
    }

    const T& operator[](usize row) const {
        return m_memory[row];
    }

    MatrixStorageVector& operator+=(const MatrixStorageVector& other) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] += other.m_memory[i];
        }
        return *this;
    }
    MatrixStorageVector& operator-=(const MatrixStorageVector& other) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] -= other.m_memory[i];
        }
        return *this;
    }
    MatrixStorageVector& operator*=(T scalar) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] *= scalar;
        }
        return *this;
    }
    MatrixStorageVector& operator/=(T scalar) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] /= scalar;
        }
        return *this;
    }
    MatrixStorageVector& operator=(const MatrixStorageVector& other) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] = other.m_memory[i];
        }
        return *this;
    }
    MatrixStorageVector& operator+=(T scalar) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] += scalar;
        }
        return *this;
    }
    MatrixStorageVector& operator-=(T scalar) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] -= scalar;
        }
        return *this;
    }
    MatrixStorageVector& operator*=(const MatrixStorageVector& other) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] *= other.m_memory[i];
        }
        return *this;
    }
    MatrixStorageVector& operator/=(const MatrixStorageVector& other) {
        for (usize i = 0; i < NRows; i++) {
            m_memory[i] /= other.m_memory[i];
        }
        return *this;
    }

    MatrixStorageVector operator+(const MatrixStorageVector& other) const {
        MatrixStorageVector result = *this;
        result += other;
        return result;
    }
    MatrixStorageVector operator-(const MatrixStorageVector& other) const {
        MatrixStorageVector result = *this;
        result -= other;
        return result;
    }
    MatrixStorageVector operator*(T scalar) const {
        MatrixStorageVector result = *this;
        result *= scalar;
        return result;
    }
    MatrixStorageVector operator/(T scalar) const {
        MatrixStorageVector result = *this;
        result /= scalar;
        return result;
    }
    MatrixStorageVector operator+(T scalar) const {
        MatrixStorageVector result = *this;
        result += scalar;
        return result;
    }
    MatrixStorageVector operator-(T scalar) const {
        MatrixStorageVector result = *this;
        result -= scalar;
        return result;
    }
    MatrixStorageVector operator*(const MatrixStorageVector& other) const {
        MatrixStorageVector result = *this;
        result *= other;
        return result;
    }
    MatrixStorageVector operator/(const MatrixStorageVector& other) const {
        MatrixStorageVector result = *this;
        result /= other;
        return result;
    }
};

template <typename T, usize NCols, usize NRows> class MatrixStorage {
public:
    using TVecType = MatrixStorageVector<T, NRows>;
    using Type = TVecType[NCols];

    Type m_memory;

    void broadcast(T x) {
        for (usize i = 0; i < NCols; i++) {
            for (usize j = 0; j < NRows; j++) {
                m_memory[i][j] = x;
            }
        }
    }
    
    TVecType& operator[](usize c) {
        return m_memory[c];
    }

    const TVecType& operator[](usize c) const {
        return m_memory[c];
    }

    T& operator[](usize row, usize col) {
        return reinterpret_cast<T*>(&m_memory[col])[row];
    }

    const T& operator[](usize row, usize col) const {
        return reinterpret_cast<const T*>(&m_memory[col])[row];
    }
};

template <typename T, usize NCols, usize NRows>
/**
 * @brief A templated class representing a matrix with fixed dimensions and various mathematical operations.
 *
 * @tparam T The type of the elements stored in the matrix.
 * @tparam NCols The number of columns in the matrix.
 * @tparam NRows The number of rows in the matrix.
 */
class Matrix {
public:
    MatrixStorage<T, NCols, NRows> m_data;

    Matrix() = default;


    Matrix(T broadcastValue) {
        m_data.broadcast(broadcastValue);
    }

    Matrix(std::initializer_list<T> ilist) {
        debug_assert(ilist.size() == NCols * NRows, "The initializer list must have the same size as the matrix.");

        auto it = ilist.begin();
        for (usize row = 0; row < NRows; row++) {
            for (usize col = 0; col < NCols; col++) {
                m_data[row, col] = *it;
                it++;
            }
        }
    }

    template <typename... Args>
        requires (sizeof...(Args) == NCols * NRows)
              && (std::is_convertible_v<Args, T> && ...)
    Matrix(Args&&... args) {
        T flat[] = { static_cast<T>(std::forward<Args>(args))... };
        for (usize c = 0; c < NCols; c++)
            for (usize r = 0; r < NRows; r++)
                m_data[r, c] = flat[c * NRows + r];
    }

    static Matrix identity() requires (NCols == NRows) {
        Matrix m{};
        for (usize i = 0; i < NCols; i++)
            m.m_data[i, i] = T{1};
        return m;
    }

    T& operator[](usize r, usize c) {
        return m_data[r, c];
    }

    const T& operator[](usize r, usize c) const {
        return m_data[r, c];
    }

    auto& column(usize c) {
        return m_data[c];
    }

    [[nodiscard]] const auto& column(usize c) const {
        return m_data[c];
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Addition / Subtraction

    Matrix operator+(const Matrix& other) const {
        Matrix result;

        for (usize col = 0; col < NCols; col++)
                result.m_data[col] = this->m_data[col] + other.m_data[col];

        return result;
    }
    Matrix& operator+=(const Matrix& other) { return *this = *this + other; }

    Matrix operator-(const Matrix& other) const {
        Matrix result;

        for (usize col = 0; col < NCols; col++)
            result.m_data[col] = this->m_data[col] - other.m_data[col];

        return result;
    }
    Matrix& operator-=(const Matrix& other) { return *this = *this - other; }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Scalar Multiplication

    Matrix operator*(T scalar) const {
        Matrix result;

        for (usize col = 0; col < NCols; col++)
            result.m_data[col] = m_data[col] * scalar;

        return result;
    }
    friend Matrix operator*(T scalar, const Matrix& m) { return m * scalar; }
    Matrix& operator*=(T scalar) { return *this = *this * scalar; }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Matrix Multiplication

    template <usize KCols>
    Matrix<T, KCols, NRows> operator*(const Matrix<T, KCols, NCols>& other) const {
        Matrix<T, KCols, NRows> result{};

        for (usize col = 0; col < KCols; col++) {
            typename MatrixStorage<T, 1, NRows>::TVecType acc{};

            for (usize vc = 0; vc < NCols; vc++) {
                T scalar = other.m_data[vc, col];
                acc += m_data[vc] * scalar;
            }

            result.m_data[col] = acc;
        }

        return result;
    }

    template <usize KCols>
    Matrix<T, KCols, NRows>& operator*=(Matrix mtx) { return *this = *this * mtx; }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Component-wise Matrix Multiplication

    [[nodiscard]] Matrix componentWiseMultiply(const Matrix& other) const {
        Matrix result;

        for (usize col = 0; col < NCols; col++)
            result.m_data[col] = m_data[col] * other.m_data[col];

        return result;
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Scalar Division

    Matrix operator/(T scalar) const {
        Matrix result;

        for (usize col = 0; col < NCols; col++)
            result.m_data[col] = m_data[col] / scalar;

        return result;
    }
    friend Matrix operator/(T scalar, const Matrix& m) {
        Matrix result;
        for (usize col = 0; col < NCols; col++)
            result.m_data[col] = scalar / m.m_data[col];
        return result;
    }
    Matrix& operator/=(T scalar) { return *this = *this / scalar; }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Component-wise Matrix Division

    [[nodiscard]] Matrix componentWiseDivide(const Matrix& other) const {
        Matrix result;

        for (usize col = 0; col < NCols; col++)
            for (usize row = 0; row < NRows; row++)
                result.m_data[col] = m_data[col] / other.m_data[col];

        return result;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------
    // Transposition

    [[nodiscard]] auto transpose() const -> Matrix<T, NRows, NCols> {
        Matrix<T, NRows, NCols> result{};
        for (usize col = 0; col < NCols; col++)
            for (usize row = 0; row < NRows; row++)
                result.m_data[row][col] = m_data[col][row];
        return result;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    bool operator==(const Matrix& other) const {
        for (usize col = 0; col < NCols; col++)
            for (usize row = 0; row < NRows; row++)
                if (m_data[col][row] != other.m_data[col][row])
                    return false;
        return true;
    }
    bool operator!=(const Matrix& other) const { return !(*this == other); }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------
    // Vector Operations

    [[nodiscard]] T& x() requires (NCols == 1 && NRows >= 1) { return m_data[0, 0]; };
    [[nodiscard]] T& y() requires (NCols == 1 && NRows >= 2) { return m_data[1, 0]; };
    [[nodiscard]] T& z() requires (NCols == 1 && NRows >= 3) { return m_data[2, 0]; };
    [[nodiscard]] T& w() requires (NCols == 1 && NRows >= 4) { return m_data[3, 0]; };


    [[nodiscard]] const T& x() const requires (NCols == 1 && NRows >= 1) { return m_data[0, 0]; };
    [[nodiscard]] const T& y() const requires (NCols == 1 && NRows >= 2) { return m_data[1, 0]; };
    [[nodiscard]] const T& z() const requires (NCols == 1 && NRows >= 3) { return m_data[2, 0]; };
    [[nodiscard]] const T& w() const requires (NCols == 1 && NRows >= 4) { return m_data[3, 0]; };

    [[nodiscard]] constexpr T sum() const requires (NCols == 1) {
        T sum = T(0);
        
        for (usize i = 0; i < NRows; i++) {
            sum += m_data[i, 0];
        }

        return sum;
    }

    [[nodiscard]] T dot(const Matrix& other) const requires (NCols == 1) {
        return componentWiseMultiply(other)
            .sum();
    }

    [[nodiscard]] Matrix<T, 1, 3> cross(const Matrix<T, 1, 3>& other) const requires (NCols == 1 && NRows == 3) {
        return {
            y() * other.z() - z() * other.y(),
            z() * other.x() - x() * other.z(),
            x() * other.y() - y() * other.x()
        };
    }

    [[nodiscard]] float lengthSquared() const requires (NCols == 1) {
        return dot(*this);
    }

    [[nodiscard]] float length() const requires (NCols == 1) {
        return std::sqrt(lengthSquared());
    }
    
    [[nodiscard]] Matrix normalize() const requires (NCols == 1) {
        return *this / length();
    }

    [[nodiscard]] Matrix round() const {
        Matrix result;
        for (usize col = 0; col < NCols; col++)
            for (usize row = 0; row < NRows; row++)
                result.m_data[col][row] = std::round(m_data[col][row]);
        return result;
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Decomposition
    [[nodiscard]] Matrix<T, 1, 3> decomposePosition() const requires (NCols == 4 && NRows == 4) {
        return {
            m_data[0, 3],
            m_data[1, 3],
            m_data[2, 3]
        };
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Matrix Inverse

    [[nodiscard]] Option<Matrix> inverse() const requires (NCols == NRows && NRows <= 4) {
        if constexpr (NRows == 1) return invertMatrix1x1(*this);
        if constexpr (NRows == 2) return invertMatrix2x2(*this);
        if constexpr (NRows == 3) return invertMatrix3x3(*this);
        if constexpr (NRows == 4) return invertMatrix4x4(*this);

        // TODO: Add a fallback later. For the moment it's not necessary.
        return None;
    }

    [[nodiscard]] Matrix inverseRigid() const requires (NCols == 4 && NRows == 4) {
        Matrix<T, 3, 3> rotMat = {
            m_data[0, 0], m_data[0, 1], m_data[0, 2],
            m_data[1, 0], m_data[1, 1], m_data[1, 2],
            m_data[2, 0], m_data[2, 1], m_data[2, 2]
        };
        Matrix<T, 1, 3> transl = {
            m_data[0, 3],
            m_data[1, 3],
            m_data[2, 3]
        };

        auto rotInverse = rotMat.transpose(); // for rotation matrices transposition is inversion
        auto translInverse = rotInverse * transl * -1.0f;

        return {
            rotInverse[0, 0], rotInverse[0, 1], rotInverse[0, 2], translInverse[0, 0],
            rotInverse[1, 0], rotInverse[1, 1], rotInverse[1, 2], translInverse[1, 0],
            rotInverse[2, 0], rotInverse[2, 1], rotInverse[2, 2], translInverse[2, 0],
            T(0),             T(0),             T(0),             T(1)
        };
    }

    /// Inverts a 1x1 matrix.
    ///
    /// It does so by simply computing the reciprocal of the element.
    auto invertMatrix1x1(Matrix<T, 1, 1> mat) const -> Option<Matrix<T, 1, 1>> {
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
    auto invertMatrix2x2(Matrix<T, 2, 2> mat) const -> Option<Matrix<T, 2, 2>> {
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
    auto invertMatrix3x3(Matrix<T, 3, 3> mat) const -> Option<Matrix<T, 3, 3>> {
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
    auto invertMatrix4x4(Matrix<T, 4, 4> mat) const -> Option<Matrix<T, 4, 4>> {
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