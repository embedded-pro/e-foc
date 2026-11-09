#include "core/foc/position_loop/PidPositionController.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/math/Math.hpp"
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
        heldBackReference = 0.0f;
        primed = false;
    }

    PositionOutput PidPositionController::Compute(const PositionControlContext& context)
    {
        const auto error = PositionDifference(context.reference, context.measured, context.direct);
        const auto referenceStep = primed ? PositionDifference(context.reference, Radians{ previousReference }, context.direct) : error;
        const auto measuredStep = primed ? PositionDifference(context.measured, Radians{ previousMeasured }, context.direct) : 0.0f;

        previousReference = context.reference.Value();
        previousMeasured = context.measured.Value();
        primed = true;

        const auto stationary = math::Abs(context.measuredSpeed.Value()) < stationarySpeedRatio * SpeedEnvelope();
        const auto integral = stationary ? relaxationRate * (proportionalGain * error - output) : integralGain * error;
        const auto referencePart = heldBackReference + proportionalGain * referenceWeight * referenceStep;
        const auto candidate = output + referencePart - proportionalGain * measuredStep + integral;
        output = std::clamp(candidate, -1.0f, 1.0f);
        heldBackReference = HeldBack(candidate - output, referencePart);

        return { PositionOutputKind::speedReference, output * SpeedEnvelope() };
    }

    float PidPositionController::HeldBack(float clamped, float referencePart)
    {
        if (clamped * referencePart <= 0.0f)
            return 0.0f;

        return math::Copysign(std::min(math::Abs(clamped), math::Abs(referencePart)), referencePart);
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
