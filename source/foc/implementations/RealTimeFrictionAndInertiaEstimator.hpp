#pragma once

#include "foc/implementations/TransformsClarkePark.hpp"
#include "foc/interfaces/Driver.hpp"
#include "foc/interfaces/FrictionAndInertiaEstimator.hpp"
#include "numerical/estimators/RecursiveLeastSquares.hpp"
#include "source/foc/interfaces/Units.hpp"

namespace foc
{
    class RealTimeFrictionAndInertiaEstimator
        : public FrictionAndInertiaEstimator
    {
    public:
        RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency);

        Result Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque) override;

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
