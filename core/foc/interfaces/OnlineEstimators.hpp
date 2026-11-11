#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"

namespace foc
{
    // One outer-loop window: positions relative to its start; first marks the window after a restart.
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

    struct EstimateBand
    {
        float minimum;
        float maximum;
    };

    constexpr EstimateBand resistanceEstimateBand{ 0.7f, 2.0f };
    constexpr EstimateBand inductanceEstimateBand{ 0.5f, 1.5f };
    constexpr EstimateBand fluxLinkageEstimateBand{ 0.75f, 1.1f };

    struct ElectricalWindow
    {
        Volts meanAppliedVd;
        Ampere meanId;
        Ampere idAtEnd;
        float meanElectricalSpeedTimesIq;
        Ampere directCurrentReference;
    };

    // One outer-loop window of the quadrature axis; iqAtEnd is the current at its last sample, first marks the window after a restart.
    struct QuadratureWindow
    {
        Volts meanAppliedVq;
        Ampere meanIq;
        Ampere iqAtEnd;
        Ampere meanId;
        RadiansPerSecond meanElectricalSpeed;
        Ohm resistance;
        MilliHenry inductance;
        Volts phaseVoltageRange;
        bool first{ false };
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
        virtual bool HasSettled() const = 0;
    };

    class OnlineFluxLinkageEstimator
    {
    protected:
        ~OnlineFluxLinkageEstimator() = default;

    public:
        virtual void SetInitialEstimate(Weber fluxLinkage) = 0;

        virtual void Update(const QuadratureWindow& window) = 0;

        virtual Weber CurrentFluxLinkage() const = 0;
        virtual bool HasSettled() const = 0;
    };
}
