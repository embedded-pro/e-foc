#pragma once

#include "core/foc/interfaces/Units.hpp"
#include <cstdint>
#include <optional>

namespace foc
{
    class MechanicalPlant
    {
    public:
        MechanicalPlant(foc::KilogramMeterSquared inertia, foc::NewtonMeterSecondPerRadian viscousFriction, uint8_t polePairs);

        void SetLoad(foc::NewtonMeter load);
        void SetExternalTorque(foc::NewtonMeter torque);
        foc::NewtonMeter ExternalTorque() const;
        void SetCoulombFriction(foc::NewtonMeter friction);
        void SetCogging(foc::NewtonMeter amplitude, uint32_t cyclesPerRevolution);
        void Grip(std::optional<foc::RadiansPerSecond> handSpeed);

        void Step(float electricalTorque, float dt);

        foc::Radians Angle() const;
        foc::RadiansPerSecond Speed() const;
        foc::Radians ElectricalAngle() const;
        foc::RadiansPerSecond ElectricalSpeed() const;

        void Set(foc::Radians angle);
        void SetZero();
        void Reset();

    private:
        float CoggingTorque() const;
        float FreeSpeed(float electricalTorque, float dt) const;

    private:
        float inertia;
        float viscousFriction;
        uint8_t polePairs;

        foc::NewtonMeter load{ 0.0f };
        foc::NewtonMeter externalTorque{ 0.0f };
        float coulombFrictionNm{ 0.0f };
        float coggingTorqueNm{ 0.0f };
        uint32_t coggingCyclesPerRevolution{ 0 };
        std::optional<foc::RadiansPerSecond> grip;

        foc::Radians theta{ 0.0f };
        foc::Radians thetaMech{ 0.0f };
        foc::RadiansPerSecond omega{ 0.0f };
        foc::RadiansPerSecond omegaMech{ 0.0f };
    };
}
