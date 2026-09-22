#include "core/foc/position_loop/PidPositionController.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"

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
        positionPid.Reset();
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    PositionOutput PidPositionController::Compute(const PositionControlContext& context)
    {
        positionPid.SetPoint(WrappedPositionError(context.reference, context.measured));

        return { PositionOutputKind::speedReference, positionPid.Process(0.0f) * SpeedEnvelope() };
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

        const auto kp = 1.0f / maximumErrorInRadians;
        const auto ki = kp * WeightRatio(tunings.integralWeight, tunings.positionErrorWeight) * OuterSamplePeriod(samplingFrequency) * tunings.bandwidth;

        positionPid.SetTunings({ kp, ki, 0.0f });
        return true;
    }
}
