#pragma once

#include "numerical/estimators/Estimator.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Units.hpp"

namespace services
{
    class FrictionAndInertiaEstimator
    {
    public:
        struct Result
        {
            foc::NewtonMeterSecondSquared inertia;
            foc::NewtonMeterSecondPerRadian friction;
            estimators::OnlineEstimator<float, 3>::EstimationMetrics metrics;
        };

        virtual Result Update(foc::PhaseCurrents currentPhases, foc::RadiansPerSecond speed, foc::Radians electricalAngle, foc::NewtonMeter targetTorque) = 0;
    };
}
