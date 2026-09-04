export module projnekomata:graphics.corematerial;
import projnekomata.corelib;
import :core.math;
import :graphics.texturesystem.texture_manager;

export namespace projnekomata {

class CoreMaterialProps {
public:
    constexpr static u32 kFlagColorIsTex = 1 << 0;
    constexpr static u32 kFlagRoughnessIsTex = 1 << 1;
    constexpr static u32 kFlagMetallicIsTex = 1 << 2;
    constexpr static u32 kFlagHasNormalMap = 1 << 3;
    constexpr static u32 kFlagHasEmissiveMap = 1 << 4;

    CoreMaterialProps() = default;

    constexpr auto setColor(math::Vector3f color) -> CoreMaterialProps& {
        colorOrTex = color;
        flags &= ~kFlagColorIsTex;
        return *this;
    }
    constexpr auto setColor(projnekomata::Texture tex) -> CoreMaterialProps& {
        colorOrTex.x() = std::bit_cast<f32>(tex.index);
        flags |= kFlagColorIsTex;
        return *this;
    }
    constexpr auto setEmissive(math::Vector3f emissive) -> CoreMaterialProps& {
        emissiveOrTex = emissive;
        flags &= ~kFlagHasEmissiveMap;
        return *this;
    }
    constexpr auto setEmissive(projnekomata::Texture tex) -> CoreMaterialProps& {
        emissiveOrTex.x() = std::bit_cast<f32>(tex.index);
        flags |= kFlagHasEmissiveMap;
        return *this;
    }
    constexpr auto setRoughness(f32 roughness) -> CoreMaterialProps& {
        roughnessOrTex = roughness;
        flags &= ~kFlagRoughnessIsTex;
        return *this;
    }
    constexpr auto setRoughness(projnekomata::Texture tex) -> CoreMaterialProps& {
        roughnessOrTex = std::bit_cast<f32>(tex.index);
        flags |= kFlagRoughnessIsTex;
        return *this;
    }
    constexpr auto setMetallic(f32 metallic) -> CoreMaterialProps& {
        metallicOrTex = metallic;
        flags &= ~kFlagMetallicIsTex;
        return *this;
    }
    constexpr auto setMetallic(projnekomata::Texture tex) -> CoreMaterialProps& {
        metallicOrTex = std::bit_cast<f32>(tex.index);
        flags |= kFlagMetallicIsTex;
        return *this;
    }
    constexpr auto setNormalMap(projnekomata::Texture tex) -> CoreMaterialProps& {
        normalMapTex = std::bit_cast<u32>(tex.index);
        flags |= kFlagHasNormalMap;
        return *this;
    }

private:
    u32 flags = 0;

    math::Vector3f colorOrTex = math::Vector3f(1.0f, 1.0f, 1.0f);
    math::Vector3f emissiveOrTex = math::Vector3f(0.0f, 0.0f, 0.0f);
    f32 roughnessOrTex = 1.0f;
    f32 metallicOrTex = 0.0f;
    u32 normalMapTex = 0;
};

}
