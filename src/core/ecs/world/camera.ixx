export module projnekomata:core.ecs.world.camera;
import std;
import :core.math;

using namespace projnekomata::math;

export namespace projnekomata {

struct CameraComponent {
    CameraComponent() = default;
    CameraComponent(float nearPlane, float farPlane, float fov, bool renderingEnable) : nearPlane(nearPlane), farPlane(farPlane), fov(fov), renderingEnable(renderingEnable) {}

    float nearPlane;
    float farPlane;
    float fov;

    bool renderingEnable;
    
    /// Computes the perspective projection matrix for use in Vulkan rendering with a reverse-Z buffer.
    auto computeProjectionMatrix(float aspectRatio) const -> Matrix4x4f {
        float f = 1.0f / std::tan(degreesToRadians(fov) / 2.0f);

        float m22 = nearPlane / (farPlane - nearPlane);
        float m23 = (farPlane * nearPlane) / (farPlane - nearPlane);

        return {
            -f / aspectRatio, 0.0f, 0.0f,  0.0f,
            0.0f,             -f,   0.0f,  0.0f,
            0.0f,             0.0f, m22,   m23,
            0.0f,             0.0f, -1.0f, 0.0f
        };
    }
};


}