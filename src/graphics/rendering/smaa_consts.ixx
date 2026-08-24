module;
#include <stddef.h>
export module projnekomata:graphics.rendering.smaa_consts;
import projnekomata.corelib;
import :graphics.vulkan.vk_spec_constants;

export namespace projnekomata::gfx {

class SmaaSpecializationConsts : public vkrhi::SpecializationConstsReflect<SmaaSpecializationConsts> {
public:
    constexpr explicit SmaaSpecializationConsts(f32 threshold = 0.15f, u32 maxSearchSteps = 4, u32 maxSearchStepsDiagonal = 0, u32 maxCornerRounding = 0)
        : threshold(threshold), maxSearchSteps(maxSearchSteps), maxSearchStepsDiagonal(maxSearchStepsDiagonal), maxCornerRounding(maxCornerRounding) {}

    f32 threshold              = 0.15f;
    u32 maxSearchSteps         = 4;
    u32 maxSearchStepsDiagonal = 0;
    u32 maxCornerRounding      = 0;

    constexpr static auto describe() -> Slice<const vk::SpecializationMapEntry> {
        static auto data = StaticSlice<const vk::SpecializationMapEntry>::inst<
            { 1000, offsetof(SmaaSpecializationConsts, threshold), sizeof(threshold) },
            { 1001, offsetof(SmaaSpecializationConsts, maxSearchSteps), sizeof(maxSearchSteps) },
            { 1002, offsetof(SmaaSpecializationConsts, maxSearchStepsDiagonal), sizeof(maxSearchStepsDiagonal) },
            { 1003, offsetof(SmaaSpecializationConsts, maxCornerRounding), sizeof(maxCornerRounding) }
        >();
        return data;
    }
};

class SmaaSpecializationConstantPresets {
public:
    static inline auto kLow = SmaaSpecializationConsts();
    static inline auto kMedium = SmaaSpecializationConsts(0.1f, 8);
    static inline auto kHigh = SmaaSpecializationConsts(0.1f, 16, 8, 25);
    static inline auto kUltra = SmaaSpecializationConsts(0.05f, 32, 16, 25);
};

}