#pragma once

#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

namespace foc
{
    struct PositionPlantModel
    {
        float speedDecay{ 1.0f };
        float currentPerNormalizedInput{ 0.0f };

        static PositionPlantModel FromParameters(const MechanicalModelParameters& parameters);
    };

    float NormalizedEffortWeight(float bandwidth, hal::Hertz samplingFrequency);
    float WeightRatio(float weight, float positionErrorWeight);

    ALWAYS_INLINE_HOT float WrappedPositionError(Radians reference, Radians measured)
    {
        constexpr float pi = std::numbers::pi_v<float>;

        const auto error = reference.Value() - measured.Value();

        if (error > pi || error < -pi)
            return detail::PositionWithWrapAround(error);

        return error;
    }
}
