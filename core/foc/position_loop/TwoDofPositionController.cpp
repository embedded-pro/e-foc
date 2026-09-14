#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/position_loop/TwoDofPositionController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/math/Math.hpp"

namespace foc
{
    bool TwoDofPositionController::Configure(const MechanicalModelParameters& motorParameters)
    {
        const bool ok = feedback.Configure(motorParameters);
        samplingFrequency = motorParameters.samplingFrequency;
        ApplyReferenceFilter();
        return ok;
    }

    bool TwoDofPositionController::SetTunings(const PositionLoopTunings& tunings)
    {
        const bool ok = feedback.SetTunings(tunings);
        referenceTimeConstant = tunings.referenceTimeConstant;
        ApplyReferenceFilter();
        return ok;
    }

    void TwoDofPositionController::Reset()
    {
        feedback.Reset();
        seeded = false;
    }

    void TwoDofPositionController::ApplyReferenceFilter()
    {
        if (samplingFrequency.Value() == 0 || referenceTimeConstant <= 0.0f)
        {
            alpha = 1.0f;
            return;
        }

        alpha = 1.0f - math::Exp(-OuterSamplePeriod(samplingFrequency) / referenceTimeConstant);
    }
}
