#include "core/foc/position_loop/TwoDofPositionController.hpp"
#include "numerical/math/Math.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    void TwoDofPositionController::Configure(const MechanicalModelParameters& motorParameters)
    {
        feedback.Configure(motorParameters);
        samplingFrequency = motorParameters.samplingFrequency;
        ApplyReferenceFilter();
    }

    void TwoDofPositionController::SetTunings(const PositionLoopTunings& tunings)
    {
        feedback.SetTunings(tunings);
        referenceTimeConstant = tunings.referenceTimeConstant;
        ApplyReferenceFilter();
    }

    void TwoDofPositionController::Reset()
    {
        feedback.Reset();
        referenceFilter.Reset();
    }

    // Pre-filtering the setpoint shapes tracking without touching the feedback gains that set load stiffness
    void TwoDofPositionController::ApplyReferenceFilter()
    {
        if (samplingFrequency.Value() == 0 || referenceTimeConstant <= 0.0f)
        {
            referenceFilter.SetAlpha(1.0f);
            return;
        }

        referenceFilter.SetAlpha(1.0f - math::Exp(-OuterSamplePeriod(samplingFrequency) / referenceTimeConstant));
    }
}
