#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/position_loop/TwoDofPositionController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/math/Math.hpp"

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
        seeded = false;
    }

    void TwoDofPositionController::ApplyReferenceFilter()
    {
        filterActive = samplingFrequency.Value() != 0 && referenceTimeConstant > 0.0f;

        if (!filterActive)
        {
            alpha = 1.0f;
            return;
        }

        alpha = 1.0f - math::Exp(-OuterSamplePeriod(samplingFrequency) / referenceTimeConstant);
    }
}
