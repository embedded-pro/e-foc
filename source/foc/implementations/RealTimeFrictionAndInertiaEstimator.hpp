#pragma once

#include "foc/implementations/TransformsClarkePark.hpp"
#include "foc/interfaces/Driver.hpp"
#include "numerical/estimators/RecursiveLeastSquares.hpp"
#include "source/foc/interfaces/Units.hpp"

namespace foc
{
    class RealTimeFrictionAndInertiaEstimator
    {
    public:
        struct Result
        {
            foc::NewtonMeterSecondPerRadian inertia;
            foc::NewtonMeterSecondSquared friction;
            estimators::OnlineEstimator<float, 3>::EstimationMetrics metrics;
        };

        RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency);

        Result Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque);

    private:
        using MotorRLS = estimators::RecursiveLeastSquares<float, 3>;
        hal::Hertz samplingFrequency;
        std::optional<MotorRLS> rls;
        [[no_unique_address]] foc::ClarkePark transform;
        MotorRLS::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        RadiansPerSecond previousSpeed{ 0.0f };
    };
}
