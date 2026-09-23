#include "core/foc/position_loop/PidPositionController.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include <algorithm>

namespace foc
{
    bool PidPositionController::Configure(const MechanicalModelParameters& motorParameters)
    {
        samplingFrequency = motorParameters.samplingFrequency;
        return ApplyGains();
    }

    bool PidPositionController::SetTunings(const PositionLoopTunings& controllerTunings)
    {
        tunings = controllerTunings;
        return ApplyGains();
    }

    void PidPositionController::Reset()
    {
        output = 0.0f;
        primed = false;
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    PositionOutput PidPositionController::Compute(const PositionControlContext& context)
    {
        const auto error = WrappedPositionError(context.reference, context.measured);
        // Before the first sample the rotor is taken as settled where it stands, so the whole error is a reference step
        const auto referenceStep = primed ? WrappedPositionError(context.reference, Radians{ previousReference }) : error;
        const auto measuredStep = primed ? WrappedPositionError(context.measured, Radians{ previousMeasured }) : 0.0f;

        previousReference = context.reference.Value();
        previousMeasured = context.measured.Value();
        primed = true;

        output = std::clamp(output + proportionalGain * (referenceWeight * referenceStep - measuredStep) + integralGain * error, -1.0f, 1.0f);

        return { PositionOutputKind::speedReference, output * SpeedEnvelope() };
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

    float PidPositionController::SpeedEnvelope() const
    {
        return tunings.bandwidth * maximumErrorInRadians;
    }

    bool PidPositionController::ApplyGains()
    {
        if (samplingFrequency.Value() == 0)
            return false;

        proportionalGain = 1.0f / maximumErrorInRadians;
        integralGain = proportionalGain * integralZeroRatio * tunings.bandwidth * OuterSamplePeriod(samplingFrequency);
        return true;
    }
}
