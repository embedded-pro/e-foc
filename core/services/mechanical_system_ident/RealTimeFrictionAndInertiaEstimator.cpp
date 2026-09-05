#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <cmath>

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

        previousSpeed = speed;

        if (!IsPersistentlyExciting(acceleration, speed.Value()))
            return Result{
                foc::NewtonMeterSecondSquared{ rls->Coefficients().at(1, 0) },
                foc::NewtonMeterSecondPerRadian{ rls->Coefficients().at(2, 0) },
                lastMetrics
            };

        MotorRLS::MakeRegressor(regressor, acceleration, speed.Value());

        torque.at(0, 0) = rotatingFrame.q * targetTorque.Value();

        lastMetrics = rls->Update(regressor, torque);

        return Result{
            foc::NewtonMeterSecondSquared{ rls->Coefficients().at(1, 0) },
            foc::NewtonMeterSecondPerRadian{ rls->Coefficients().at(2, 0) },
            lastMetrics
        };
    }

    bool RealTimeFrictionAndInertiaEstimator::IsPersistentlyExciting(float acceleration, float speed) const
    {
        return std::abs(acceleration) >= minimumAcceleration || std::abs(speed) >= minimumSpeed;
    }

    bool RealTimeFrictionAndInertiaEstimator::IsPlausible(float inertia, float friction)
    {
        return foc::IsFiniteValue(inertia) && foc::IsFiniteValue(friction) &&
               inertia > minimumInertia && inertia < maximumInertia &&
               friction >= 0.0f && friction < maximumFriction;
    }

    void RealTimeFrictionAndInertiaEstimator::Seed(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction)
    {
        if (!rls.has_value())
            return;

        // RLS model: torque = theta[0]*1(coulomb) + theta[1]*acceleration(inertia) + theta[2]*speed(friction)
        MotorRLS::CoefficientsMatrix initial{};
        initial.at(0, 0) = 0.0f;
        initial.at(1, 0) = inertia.Value();
        initial.at(2, 0) = friction.Value();
        rls->SetCoefficients(initial);
    }

    void RealTimeFrictionAndInertiaEstimator::SetTorqueConstant(foc::NewtonMeter kt)
    {
        torqueConstant = kt;
    }

    void RealTimeFrictionAndInertiaEstimator::SetInitialEstimate(
        foc::NewtonMeterSecondSquared inertia,
        foc::NewtonMeterSecondPerRadian friction)
    {
        currentInertia = inertia;
        currentFriction = friction;
        Seed(inertia, friction);
    }

    void RealTimeFrictionAndInertiaEstimator::Update(
        foc::PhaseCurrents currentPhases,
        foc::RadiansPerSecond speed,
        foc::Radians electricalAngle)
    {
        auto result = Update(currentPhases, speed, electricalAngle, torqueConstant);

        if (!IsPlausible(result.inertia.Value(), result.friction.Value()))
            return;

        currentInertia = result.inertia;
        currentFriction = result.friction;
    }

    foc::NewtonMeterSecondSquared RealTimeFrictionAndInertiaEstimator::CurrentInertia() const
    {
        return currentInertia;
    }

    foc::NewtonMeterSecondPerRadian RealTimeFrictionAndInertiaEstimator::CurrentFriction() const
    {
        return currentFriction;
    }
}
