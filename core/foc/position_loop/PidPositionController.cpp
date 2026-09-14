#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/position_loop/PidPositionController.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"

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
        positionPid.SetPoint(WrappedPositionError(context.reference, context.measured));

        return { PositionOutputKind::speedReference, positionPid.Process(0.0f) * SpeedEnvelope() };
    }

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
