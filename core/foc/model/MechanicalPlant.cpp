#include "core/foc/model/MechanicalPlant.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include <cmath>

namespace foc
{
    MechanicalPlant::MechanicalPlant(foc::KilogramMeterSquared inertia, foc::NewtonMeterSecondPerRadian viscousFriction, uint8_t polePairs)
        : inertia(inertia.Value())
        , viscousFriction(viscousFriction.Value())
        , polePairs(polePairs)
    {}

    void MechanicalPlant::SetLoad(foc::NewtonMeter torque)
    {
        load = torque;
    }

    void MechanicalPlant::SetExternalTorque(foc::NewtonMeter torque)
    {
        externalTorque = torque;
    }

    foc::NewtonMeter MechanicalPlant::ExternalTorque() const
    {
        return externalTorque;
    }

    void MechanicalPlant::SetCoulombFriction(foc::NewtonMeter friction)
    {
        coulombFrictionNm = friction.Value();
    }

    void MechanicalPlant::SetCogging(foc::NewtonMeter amplitude, uint32_t cyclesPerRevolution)
    {
        coggingTorqueNm = amplitude.Value();
        coggingCyclesPerRevolution = cyclesPerRevolution;
    }

    void MechanicalPlant::Grip(std::optional<foc::RadiansPerSecond> handSpeed)
    {
        grip = handSpeed;
    }

    void MechanicalPlant::Step(float electricalTorque, float dt)
    {
        // A gripping hand is stronger than any drive: the shaft goes where the hand takes it
        omegaMech = grip.has_value() ? *grip : foc::RadiansPerSecond{ FreeSpeed(electricalTorque, dt) };
        omega = foc::RadiansPerSecond{ polePairs * omegaMech.Value() };

        thetaMech += foc::Radians{ omegaMech.Value() * dt };
        theta += foc::Radians{ omega.Value() * dt };

        thetaMech = foc::Radians{ detail::PositionWithWrapAround(thetaMech.Value()) };
        theta = foc::Radians{ detail::PositionWithWrapAround(theta.Value()) };
    }

    foc::Radians MechanicalPlant::Angle() const
    {
        return thetaMech;
    }

    foc::RadiansPerSecond MechanicalPlant::Speed() const
    {
        return omegaMech;
    }

    foc::Radians MechanicalPlant::ElectricalAngle() const
    {
        return theta;
    }

    foc::RadiansPerSecond MechanicalPlant::ElectricalSpeed() const
    {
        return omega;
    }

    void MechanicalPlant::Set(foc::Radians angle)
    {
        thetaMech = angle;
        theta = foc::Radians{ polePairs * thetaMech.Value() };
    }

    void MechanicalPlant::SetZero()
    {
        thetaMech = foc::Radians{ 0.0f };
        theta = foc::Radians{ 0.0f };
    }

    void MechanicalPlant::Reset()
    {
        grip = std::nullopt;
        theta = foc::Radians{ 0.0f };
        thetaMech = foc::Radians{ 0.0f };
        omega = foc::RadiansPerSecond{ 0.0f };
        omegaMech = foc::RadiansPerSecond{ 0.0f };
    }

    float MechanicalPlant::FreeSpeed(float electricalTorque, float dt) const
    {
        const auto loadOpposing = (omegaMech.Value() >= 0.0f) ? load.Value() : -load.Value();
        const auto acceleration = (electricalTorque - viscousFriction * omegaMech.Value() - loadOpposing - externalTorque.Value() - CoggingTorque()) / inertia;

        float speed = omegaMech.Value() + acceleration * dt;

        if (coulombFrictionNm > 0.0f)
        {
            const float frictionStep = coulombFrictionNm * dt / inertia;
            speed = std::fabs(speed) <= frictionStep ? 0.0f : speed - std::copysign(frictionStep, speed);
        }

        return speed;
    }

    float MechanicalPlant::CoggingTorque() const
    {
        if (coggingTorqueNm == 0.0f)
            return 0.0f;

        return coggingTorqueNm * foc::FastTrigonometry::Sine(static_cast<float>(coggingCyclesPerRevolution) * thetaMech.Value());
    }
}
