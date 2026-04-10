#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/foc/implementations/TrigonometricImpl.hpp"
#include "core/foc/interfaces/Units.hpp"

namespace services
{
    RealTimeFrictionAndInertiaEstimator::RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency)
        : samplingFrequency(static_cast<float>(samplingFrequency.Value()))
        , rls(MotorRLS{ 1000.0f, forgettingFactor })
    {
    }

    RealTimeFrictionAndInertiaEstimator::Result RealTimeFrictionAndInertiaEstimator::Update(foc::PhaseCurrents currentPhases, foc::RadiansPerSecond speed, foc::Radians electricalAngle, foc::NewtonMeter targetTorque)
    {
        auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle.Value()), foc::FastTrigonometry::Sine(electricalAngle.Value()));
        auto acceleration = (speed.Value() - previousSpeed.Value()) * samplingFrequency;
        MotorRLS::MakeRegressor(regressor, acceleration, speed.Value());

        torque.at(0, 0) = rotatingFrame.q * targetTorque.Value();

        auto metrics = rls->Update(regressor, torque);

        previousSpeed = speed;

        return Result{
            foc::NewtonMeterSecondSquared{ rls->Coefficients().at(1, 0) },
            foc::NewtonMeterSecondPerRadian{ rls->Coefficients().at(2, 0) },
            metrics
        };
    }
}
