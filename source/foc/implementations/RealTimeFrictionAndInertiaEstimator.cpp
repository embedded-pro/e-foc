#include "source/foc/implementations/RealTimeFrictionAndInertiaEstimator.hpp"
#include "foc/interfaces/Units.hpp"
#include "source/foc/instantiations/TrigonometricImpl.hpp"

namespace foc
{
    RealTimeFrictionAndInertiaEstimator::RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency)
        : samplingFrequency(samplingFrequency)
        , rls(MotorRLS{ 1000.0f, forgettingFactor })
    {
    }

    RealTimeFrictionAndInertiaEstimator::Result RealTimeFrictionAndInertiaEstimator::Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque)
    {
        auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle.Value()), foc::FastTrigonometry::Sine(electricalAngle.Value()));
        auto acceleration = (speed.Value() - previousSpeed.Value()) * samplingFrequency.Value();
        MotorRLS::MakeRegressor(regressor, acceleration, speed.Value());

        torque.at(0, 0) = rotatingFrame.q * targetTorque.Value();

        auto metrics = rls->Update(regressor, torque);

        previousSpeed = speed;

        return Result{
            foc::NewtonMeterSecondPerRadian{ rls->Coefficients().at(2, 0) },
            foc::NewtonMeterSecondSquared{ rls->Coefficients().at(1, 0) },
            metrics
        };
    }
}
