#pragma once

#include "foc/interfaces/Driver.hpp"
#include "numerical/estimators/Estimator.hpp"
#include "source/foc/interfaces/Units.hpp"

namespace foc
{
    class FrictionAndInertiaEstimator
    {
    public:
        struct Result
        {
            foc::NewtonMeterSecondPerRadian inertia;
            foc::NewtonMeterSecondSquared friction;
            estimators::OnlineEstimator<float, 3>::EstimationMetrics metrics;
        };

        virtual ~FrictionAndInertiaEstimator() = default;

        virtual Result Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque) = 0;
    };
}
