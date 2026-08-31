export module projnekomata:core.math.transform3d;
import :core.math.quaternion;
import :core.math.matrix_types;

export namespace projnekomata::math {
    
class Transform3D {
public:
    Transform3D() = default;
    Transform3D(Vector3f position, Quaternion rotation, Vector3f scale) : m_position(position), m_rotation(rotation), m_scale(scale) {}
    Vector3f m_position;
    Quaternion m_rotation{};
    Vector3f m_scale;

    static auto identity() -> Transform3D {
        return {Vector3f(0.0f), Quaternion::identity(), Vector3f(1.0f)};
    }

    /// Constructs the model matrix of the object from its position, rotation, and scale.
    [[nodiscard]] auto computeModelMatrix() const -> Matrix4x4f {
        auto mat = m_rotation.toRotationMatrix();

        for (usize i = 0; i < 3; i++) {
            mat[i, 0] *= m_scale.x();
            mat[i, 1] *= m_scale.y();
            mat[i, 2] *= m_scale.z();
        }

        mat[0, 3] = m_position.x();
        mat[1, 3] = m_position.y();
        mat[2, 3] = m_position.z();
        mat[3, 3] = 1.0f;
        return mat;
    } 
};

}