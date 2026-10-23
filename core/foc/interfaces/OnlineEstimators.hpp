#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"

namespace foc
{
    // One outer-loop window of the control interrupt. Positions are relative to the window's start, the previous
    // window's last sample; u = (sample + 1/2) / samples is a sample's place in the window, and first marks the
    // window after a restart, which continues no earlier one.
    struct MechanicalWindow
    {
        Ampere meanIq{ 0.0f };
        float meanIqTimesU{ 0.0f };
        float meanIqTimesUSquared{ 0.0f };
        Radians meanPosition{ 0.0f };
        Radians rotation{ 0.0f };
        Radians smallestStep{ 0.0f };
        bool first{ true };
    };

    struct ElectricalWindow
    {
        Volts meanAppliedVd;
        Ampere meanId;
        Ampere idAtEnd;
        float meanElectricalSpeedTimesIq;
        Ampere directCurrentReference;
    };

    class OnlineMechanicalEstimator
    {
    protected:
        ~OnlineMechanicalEstimator() = default;

    public:
        virtual void SetTorqueConstant(NewtonMeter kt) = 0;
        virtual void SetInitialEstimate(NewtonMeterSecondSquared inertia, NewtonMeterSecondPerRadian friction) = 0;

        virtual void Update(const MechanicalWindow& window) = 0;

        virtual NewtonMeterSecondSquared CurrentInertia() const = 0;
        virtual NewtonMeterSecondPerRadian CurrentFriction() const = 0;
    };

    class OnlineElectricalEstimator
    {
    protected:
        ~OnlineElectricalEstimator() = default;

    public:
        virtual void SetInitialEstimate(Ohm resistance, MilliHenry inductance) = 0;

        virtual void Update(const ElectricalWindow& window) = 0;

        virtual Ohm CurrentResistance() const = 0;
        virtual MilliHenry CurrentInductance() const = 0;
    };
}
