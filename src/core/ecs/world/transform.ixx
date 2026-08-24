export module projnekomata:core.ecs.world.transform;
import :core.math;

using namespace projnekomata::math;

export namespace projnekomata {

class LocalTransformComponent {
public:
    LocalTransformComponent() = default;
    LocalTransformComponent(math::Vector3f position, math::Quaternion rotation, math::Vector3f scale) : m_transform3d(position, rotation, scale) {}

    Transform3D m_transform3d;
};

class WorldTransformComponent {
public:
    WorldTransformComponent() = default;
    WorldTransformComponent(math::Matrix4x4f transform) : m_transform(transform) {}

    math::Matrix4x4f m_transform = math::Matrix4x4f::identity();
};


}