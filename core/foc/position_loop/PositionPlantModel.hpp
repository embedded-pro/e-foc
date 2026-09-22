#pragma once

#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cmath>
#include <numbers>

namespace foc
{
    struct PositionPlantModel
    {
        float speedDecay{ 1.0f };
        float currentPerNormalizedInput{ 0.0f };

        static PositionPlantModel FromParameters(const MechanicalModelParameters& parameters);
    };

    float WeightRatio(float weight, float positionErrorWeight);

    ALWAYS_INLINE_HOT float WrappedPositionError(Radians reference, Radians measured)
    {
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        return std::remainder(reference.Value() - measured.Value(), two_pi);
    }
}
