#include "core/foc/speed_loop/PidSpeedController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"

namespace foc
{
    bool PidSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        parameters = motorParameters;
        return ApplyGains();
    }

    bool PidSpeedController::SetTunings(const SpeedLoopTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        return ApplyGains();
    }

    void PidSpeedController::Reset()
    {
        speedPid.Reset();
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    foc::Ampere PidSpeedController::Compute(const SpeedControlContext& context)
    {
        speedPid.SetPoint(context.reference.Value());

        return LimitToCurrentEnvelope(speedPid.Process(context.measured.Value()) * parameters.maxCurrent.Value(), parameters.maxCurrent);
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

    bool PidSpeedController::ApplyGains()
    {
        if (!AreMechanicalParametersValid(parameters))
            return false;

        const auto inertia = parameters.inertia.Value();
        const auto scale = 1.0f / parameters.maxCurrent.Value();
        const auto kp = 2.0f * inertia * bandwidth / parameters.torqueConstant.Value();
        const auto integralZero = std::max(parameters.viscousFriction.Value() / inertia, bandwidth / 4.0f);
        const auto ki = kp * integralZero * OuterSamplePeriod(parameters.samplingFrequency);

        speedPid.SetTunings({ kp * scale, ki * scale, 0.0f });
        return true;
    }
}
