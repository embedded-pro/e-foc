#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"

namespace foc
{
    // Averages over one outer-loop period, accumulated by the control interrupt, so the regression sees what
    // the plant integrated rather than one instantaneous sample of it
    struct MechanicalWindow
    {
        Ampere meanIq;
        RadiansPerSecond meanSpeed;
    };

    struct ElectricalWindow
    {
        Volts meanAppliedVd;
        Ampere meanId;
        Ampere idAtEnd;
        float meanElectricalSpeedTimesIq;
    };

    class OnlineMechanicalEstimator
    {
    public:
        virtual ~OnlineMechanicalEstimator() = default;

        virtual void SetTorqueConstant(NewtonMeter kt) = 0;
        virtual void SetInitialEstimate(NewtonMeterSecondSquared inertia, NewtonMeterSecondPerRadian friction) = 0;

        virtual void Update(const MechanicalWindow& window) = 0;

        virtual NewtonMeterSecondSquared CurrentInertia() const = 0;
        virtual NewtonMeterSecondPerRadian CurrentFriction() const = 0;
    };

    class OnlineElectricalEstimator
    {
    public:
        virtual ~OnlineElectricalEstimator() = default;

        virtual void SetInitialEstimate(Ohm resistance, MilliHenry inductance) = 0;

        virtual void Update(const ElectricalWindow& window) = 0;

        virtual Ohm CurrentResistance() const = 0;
        virtual MilliHenry CurrentInductance() const = 0;
    };
}
