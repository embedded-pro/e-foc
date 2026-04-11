#include "core/foc/implementations/FocWithSpeedLoop.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"

namespace foc
{
    void FocWithSpeedLoop::UpdateOnlineMechanicalEstimator(float mechanicalSpeed)
    {
        if (onlineMechEstimator == nullptr)
            return;

        onlineMechEstimator->Update(
            lastPhaseCurrents,
            RadiansPerSecond{ mechanicalSpeed },
            Radians{ lastElectricalAngle });
    }

    void FocWithSpeedLoop::UpdateOnlineElectricalEstimator(float electricalSpeed)
    {
        if (onlineElecEstimator == nullptr)
            return;

        const float physicalVd = lastNormalizedVd * vdcInvScale;
        onlineElecEstimator->Update(
            Volts{ physicalVd },
            Ampere{ lastMeasuredId },
            Ampere{ lastMeasuredIq },
            RadiansPerSecond{ electricalSpeed });
    }
}
