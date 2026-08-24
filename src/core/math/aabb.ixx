export module projnekomata:core.math.aabb;
import projnekomata.corelib;
import :core.math.matrix_types;

export namespace projnekomata::math {

template <typename T, usize NDims> class Aabb {
public:
    Aabb() = default;
    Aabb(const Vector<T, NDims>& min, const Vector<T, NDims>& max) : m_min(min), m_max(max) {}

    auto min() noexcept -> Vector<T, NDims>& { return m_min; }
    auto min() const noexcept -> const Vector<T, NDims>& { return m_min; }
    auto max() noexcept -> Vector<T, NDims>& { return m_max; }
    auto max() const noexcept -> const Vector<T, NDims>& { return m_max; }
    auto extents() const noexcept -> Vector<T, NDims> { return m_max - m_min; }

    auto containsExcl(const Vector<T, NDims>& point) const noexcept -> bool {
        for (usize i = 0; i < NDims; i++) {
            if (point[i] < m_min[i] || point[i] > m_max[i]) return false;
        }
        return true;
    }

    auto contains(const Vector<T, NDims>& point) const noexcept -> bool {
        for (usize i = 0; i < NDims; i++) {
            if (point[i] <= m_min[i] || point[i] >= m_max[i]) return false;
        }
        return true;
    }

private:
    Vector<T, NDims> m_min;
    Vector<T, NDims> m_max;
};

}