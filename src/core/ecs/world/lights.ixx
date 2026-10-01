export module projnekomata:core.ecs.world.pointlight;
import std;
import :core.math;

using namespace projnekomata::math;

export namespace projnekomata {

enum class LightType {
    Point,
    Spot
};

struct LightComponent {
    LightComponent() = default;
    LightComponent(LightType type, Vector3f lightRadiance, float range) : type(type), lightRadiance(lightRadiance), range(range), spotlightInnerAngle(0.0f), spotlightOuterAngle(0.0f) {}
    LightComponent(LightType type, Vector3f lightRadiance, float range, float spotlightInnerAngle, float spotlightOuterAngle) : type(type), lightRadiance(lightRadiance), range(range), spotlightInnerAngle(spotlightInnerAngle), spotlightOuterAngle(spotlightOuterAngle) {}

    LightType type;
    Vector3f lightRadiance;

    float range;
    float spotlightInnerAngle;
    float spotlightOuterAngle;

    bool castShadows;
    Vector2i shadowMapFaceSize;
};

}