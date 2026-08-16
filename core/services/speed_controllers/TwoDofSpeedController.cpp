#include "core/services/speed_controllers/TwoDofSpeedController.hpp"
#include "core/services/speed_controllers/SpeedPlantModel.hpp"
#include "numerical/math/Math.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace services
{
    void TwoDofSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        feedback.Configure(motorParameters);
        samplingFrequency = motorParameters.samplingFrequency;
        ApplyReferenceFilter();
    }

    void TwoDofSpeedController::SetTunings(const SpeedControllerTunings& tunings)
    {
        feedback.SetTunings(tunings);
        referenceTimeConstant = tunings.referenceTimeConstant;
        ApplyReferenceFilter();
    }

    void TwoDofSpeedController::Reset()
    {
        feedback.Reset();
        referenceFilter.Reset();
    }

    OPTIMIZE_FOR_SPEED
    foc::Ampere TwoDofSpeedController::Compute(const SpeedControlContext& context)
    {
        return feedback.Compute({ context.measured, foc::RadiansPerSecond{ referenceFilter.Filter(context.reference.Value()) } });
    }

    // Pre-filtering the setpoint shapes tracking without touching the feedback gains that set load stiffness
    void TwoDofSpeedController::ApplyReferenceFilter()
    {
        if (samplingFrequency.Value() == 0 || referenceTimeConstant <= 0.0f)
        {
            referenceFilter.SetAlpha(1.0f);
            return;
        }

        referenceFilter.SetAlpha(1.0f - math::Exp(-OuterSamplePeriod(samplingFrequency) / referenceTimeConstant));
    }
}
