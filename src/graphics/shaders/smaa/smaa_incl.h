#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 1
#define SMAA_HLSL_4_1
#define SMAA_REPROJECTION 1

[vk::constant_id(1000)] const float sc_smaaThreshold;
[vk::constant_id(1001)] const uint  sc_smaaMaxSearchSteps;
[vk::constant_id(1002)] const uint  sc_smaaMaxSearchStepsDiagonal;
[vk::constant_id(1003)] const uint  sc_smaaMaxCornerRounding;

#define SMAA_THRESHOLD (sc_smaaThreshold)
#define SMAA_MAX_SEARCH_STEPS (sc_smaaMaxSearchSteps)
#define SMAA_MAX_SEARCH_STEPS_DIAG (sc_smaaMaxSearchStepsDiagonal)
#define SMAA_MAX_CORNER_ROUNDING (sc_smaaMaxCornerRounding)

const static bool kSmaaDisableCornerDetection = (sc_smaaMaxCornerRounding == 0);
const static bool kSmaaDisableDiagDetection = (sc_smaaMaxSearchStepsDiagonal == 0);

#include "smaa.hlsl"