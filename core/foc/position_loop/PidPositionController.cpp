#include "core/foc/position_loop/PidPositionController.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include <algorithm>
#include <cmath>

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

    PositionOutput PidPositionController::Compute(const PositionControlContext& context)
    {
        const auto error = WrappedPositionError(context.reference, context.measured);
        const auto referenceStep = primed ? WrappedPositionError(context.reference, Radians{ previousReference }) : error;
        const auto measuredStep = primed ? WrappedPositionError(context.measured, Radians{ previousMeasured }) : 0.0f;

        previousReference = context.reference.Value();
        previousMeasured = context.measured.Value();
        primed = true;

        const auto stationary = std::abs(context.measuredSpeed.Value()) < stationarySpeedRatio * SpeedEnvelope();
        const auto integral = stationary ? relaxationRate * (proportionalGain * error - output) : integralGain * error;
        output = std::clamp(output + proportionalGain * (referenceWeight * referenceStep - measuredStep) + integral, -1.0f, 1.0f);

        return { PositionOutputKind::speedReference, output * SpeedEnvelope() };
    }

    float PidPositionController::SpeedEnvelope() const
    {
        return tunings.bandwidth * maximumErrorInRadians;
    }

    bool PidPositionController::ApplyGains()
    {
        if (samplingFrequency.Value() == 0)
            return false;

        proportionalGain = 1.0f / maximumErrorInRadians;
        relaxationRate = integralZeroRatio * tunings.bandwidth * OuterSamplePeriod(samplingFrequency);
        integralGain = proportionalGain * relaxationRate;
        return true;
    }
}
