#include "core/foc/speed_loop/PidSpeedController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    void PidSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        parameters = motorParameters;
        ApplyGains();
    }

    void PidSpeedController::SetTunings(const SpeedLoopTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        ApplyGains();
    }

    void PidSpeedController::Reset()
    {
        speedPid.Reset();
    }

    OPTIMIZE_FOR_SPEED
    foc::Ampere PidSpeedController::Compute(const SpeedControlContext& context)
    {
        speedPid.SetPoint(context.reference.Value());

        return LimitToCurrentEnvelope(speedPid.Process(context.measured.Value()) * parameters.maxCurrent.Value(), parameters.maxCurrent);
    }

    void PidSpeedController::ApplyGains()
    {
        if (!AreMechanicalParametersValid(parameters))
            return;

        const auto inertia = parameters.inertia.Value();
        const auto scale = 1.0f / parameters.maxCurrent.Value();
        const auto kp = 2.0f * inertia * bandwidth / parameters.torqueConstant.Value();
        const auto ki = kp * parameters.viscousFriction.Value() / inertia * OuterSamplePeriod(parameters.samplingFrequency);

        speedPid.SetTunings({ kp * scale, ki * scale, 0.0f });
    }
}
