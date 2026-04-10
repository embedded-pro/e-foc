#pragma once

#include "numerical/estimators/online/RecursiveLeastSquares.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/mechanical_system_ident/FrictionAndInertiaEstimator.hpp"

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
        float samplingFrequency;
        std::optional<MotorRLS> rls;
        [[no_unique_address]] foc::ClarkePark transform;
        MotorRLS::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        foc::RadiansPerSecond previousSpeed{ 0.0f };
    };
}
