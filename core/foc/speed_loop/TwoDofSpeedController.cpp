#include "core/foc/speed_loop/TwoDofSpeedController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/math/Math.hpp"

namespace foc
{
    bool TwoDofSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        const bool ok = feedback.Configure(motorParameters);
        samplingFrequency = motorParameters.samplingFrequency;
        ApplyReferenceFilter();
        return ok;
    }

    bool TwoDofSpeedController::SetTunings(const SpeedLoopTunings& tunings)
    {
        const bool ok = feedback.SetTunings(tunings);
        referenceTimeConstant = tunings.referenceTimeConstant;
        ApplyReferenceFilter();
        return ok;
    }

    void TwoDofSpeedController::Reset()
    {
        feedback.Reset();
        referenceFilter.Reset();
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    foc::Ampere TwoDofSpeedController::Compute(const SpeedControlContext& context)
    {
        return feedback.Compute({ context.measured, foc::RadiansPerSecond{ referenceFilter.Filter(context.reference.Value()) } });
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

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
