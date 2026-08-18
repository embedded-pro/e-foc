#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/position_loop/PidPositionController.hpp"

namespace foc
{
    void PidPositionController::Configure(const MechanicalModelParameters& motorParameters)
    {
        samplingFrequency = motorParameters.samplingFrequency;
        ApplyGains();
    }

    void PidPositionController::SetTunings(const PositionLoopTunings& controllerTunings)
    {
        tunings = controllerTunings;
        ApplyGains();
    }

    void PidPositionController::Reset()
    {
        positionPid.Reset();
    }

    OPTIMIZE_FOR_SPEED
    PositionOutput PidPositionController::Compute(const PositionControlContext& context)
    {
        // Feeding the wrapped error against a zero measurement keeps the shortest path across the seam
        positionPid.SetPoint(WrappedPositionError(context.reference, context.measured));

        return { PositionOutputKind::speedReference, positionPid.Process(0.0f) * SpeedEnvelope() };
    }

    // One radian of error commands the loop bandwidth in radians per second, which is the
    // proportional law the cascade shipped with; the integral term removes the standing error.
    float PidPositionController::SpeedEnvelope() const
    {
        return tunings.bandwidth * maximumErrorInRadians;
    }

    void PidPositionController::ApplyGains()
    {
        if (samplingFrequency.Value() == 0)
            return;

        const auto kp = 1.0f / maximumErrorInRadians;
        const auto ki = kp * WeightRatio(tunings.integralWeight, tunings.positionErrorWeight) * OuterSamplePeriod(samplingFrequency) * tunings.bandwidth;

        positionPid.SetTunings({ kp, ki, 0.0f });
    }
}
