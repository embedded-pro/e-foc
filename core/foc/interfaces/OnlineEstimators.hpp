#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"

namespace foc
{
    class OnlineMechanicalEstimator
    {
    public:
        virtual ~OnlineMechanicalEstimator() = default;

        virtual void SetTorqueConstant(NewtonMeter kt) = 0;
        virtual void SetInitialEstimate(NewtonMeterSecondSquared inertia, NewtonMeterSecondPerRadian friction) = 0;

        virtual void Update(PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle) = 0;

        virtual NewtonMeterSecondSquared CurrentInertia() const = 0;
        virtual NewtonMeterSecondPerRadian CurrentFriction() const = 0;
    };

    class OnlineElectricalEstimator
    {
    public:
        virtual ~OnlineElectricalEstimator() = default;

        virtual void SetInitialEstimate(Ohm resistance, MilliHenry inductance) = 0;

        virtual void Update(Volts vd, Ampere id, Ampere iq, RadiansPerSecond electricalSpeed) = 0;

        virtual Ohm CurrentResistance() const = 0;
        virtual MilliHenry CurrentInductance() const = 0;
    };
}
