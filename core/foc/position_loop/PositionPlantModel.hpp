#pragma once

#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

namespace foc
{
    // Rigid body discretised on the time-scaled state (theta, omega * Ts) with a unit input matrix.
    // The raw (theta, omega) form is a near double integrator that the float Riccati iteration does
    // not resolve; scaling time into the state and normalising the input keeps every entry near unity.
    struct PositionPlantModel
    {
        float speedDecay{ 1.0f };
        float currentPerNormalizedInput{ 0.0f };

        static PositionPlantModel FromParameters(const MechanicalModelParameters& parameters);
    };

    // Effort weight that places the closed loop at the requested bandwidth: settling takes
    // roughly 2 / bandwidth seconds because the scaled input costs 1 / (bandwidth * Ts) squared.
    float NormalizedEffortWeight(float bandwidth, hal::Hertz samplingFrequency);

    // Weights are shape knobs only; the position entry anchors them at one so the cost always
    // penalises position, which is what makes the Riccati recursion converge on the integrator state.
    float WeightRatio(float weight, float positionErrorWeight);

    ALWAYS_INLINE_HOT float WrappedPositionError(Radians reference, Radians measured)
    {
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr float twoPi = 2.0f * pi;

        auto error = reference.Value() - measured.Value();

        if (error > pi)
            error -= twoPi;
        else if (error < -pi)
            error += twoPi;

        return error;
    }
}
