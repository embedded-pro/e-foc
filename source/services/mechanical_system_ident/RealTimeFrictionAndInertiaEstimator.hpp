#pragma once

#include "numerical/estimators/RecursiveLeastSquares.hpp"
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Units.hpp"
#include "source/services/mechanical_system_ident/FrictionAndInertiaEstimator.hpp"

namespace services
{
    class RealTimeFrictionAndInertiaEstimator
        : public FrictionAndInertiaEstimator
    {
    public:
        RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency);

        Result Update(foc::PhaseCurrents currentPhases, foc::RadiansPerSecond speed, foc::Radians electricalAngle, foc::NewtonMeter targetTorque) override;

    private:
        using MotorRLS = estimators::RecursiveLeastSquares<float, 3>;
        hal::Hertz samplingFrequency;
        std::optional<MotorRLS> rls;
        [[no_unique_address]] foc::ClarkePark transform;
        MotorRLS::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        foc::RadiansPerSecond previousSpeed{ 0.0f };
    };
}
