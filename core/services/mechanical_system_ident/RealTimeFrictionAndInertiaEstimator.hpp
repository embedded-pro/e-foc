#pragma once

#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/mechanical_system_ident/FrictionAndInertiaEstimator.hpp"
#include "numerical/estimators/online/RecursiveLeastSquares.hpp"

namespace services
{
    class RealTimeFrictionAndInertiaEstimator
        : public FrictionAndInertiaEstimator
        , public foc::OnlineMechanicalEstimator
    {
    public:
        static constexpr float defaultForgettingFactor = 0.995f;

        RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency);

        // FrictionAndInertiaEstimator (4-param, for one-shot calibration procedure)
        Result Update(foc::PhaseCurrents currentPhases, foc::RadiansPerSecond speed, foc::Radians electricalAngle, foc::NewtonMeter targetTorque) override;

        void Seed(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction);

        // OnlineMechanicalEstimator (3-param, driven by state machine each outer-loop cycle)
        void SetTorqueConstant(foc::NewtonMeter kt) override;
        void SetInitialEstimate(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction) override;

        void Update(foc::PhaseCurrents currentPhases, foc::RadiansPerSecond speed, foc::Radians electricalAngle) override;

        foc::NewtonMeterSecondSquared CurrentInertia() const override;
        foc::NewtonMeterSecondPerRadian CurrentFriction() const override;

    private:
        using MotorRLS = estimators::RecursiveLeastSquares<float, 3>;
        float samplingFrequency;
        std::optional<MotorRLS> rls;
        [[no_unique_address]] foc::ClarkePark transform;
        MotorRLS::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        foc::RadiansPerSecond previousSpeed{ 0.0f };

        foc::NewtonMeter torqueConstant{ 1.0f };
        foc::NewtonMeterSecondSquared currentInertia{ 0.0f };
        foc::NewtonMeterSecondPerRadian currentFriction{ 0.0f };
    };
}
