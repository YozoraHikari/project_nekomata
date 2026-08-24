export module projnekomata:core.vertex;
import :core.math;

export namespace projnekomata {
struct Vertex {
    math::Vector3f position;
    math::Vector3f normal;
    math::Vector4f tangent;
    math::Vector2f texcoord;
    math::Vector4f color;
};
}