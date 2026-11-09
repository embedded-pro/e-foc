#include "core/foc/model/ElectricalPlant.hpp"
#include "numerical/math/FastTrigonometry.hpp"

namespace foc
{
    namespace
    {
        constexpr float half = 0.5f;
    }

    void ElectricalPlant::SetPowerStage(const PowerStageConfig& config)
    {
        powerStage = config;
    }

    const PowerStageConfig& ElectricalPlant::PowerStage() const
    {
        return powerStage;
    }

    void ElectricalPlant::SetOpenPhases(const OpenPhases& phases)
    {
        openPhases = phases;
    }

    foc::ThreePhase ElectricalPlant::PhaseVoltages(const foc::PhasePwmDutyCycles& duties, foc::Volts bus, float period) const
    {
        const auto busVoltage = bus.Value();
        const auto leg = [this, period, busVoltage](hal::DutyCycle duty, foc::Ampere current)
        {
            return PowerStageLegVoltage(powerStage, foc::DutyFraction(duty), current.Value(), busVoltage, period) - half * busVoltage;
        };

        return foc::ThreePhase{ leg(duties.a, ia), leg(duties.b, ib), leg(duties.c, ic) };
    }

    foc::RotatingFrame ElectricalPlant::Step(const foc::ThreePhase& phaseVoltages, foc::Radians theta, foc::RadiansPerSecond omega, const WindingParameters& winding, float dt)
    {
        const auto stepRotation = omega.Value() * dt;
        const auto cosTheta = math::FastCosine(theta.Value());
        const auto sinTheta = math::FastSine(theta.Value());
        const auto midAngle = theta.Value() + 0.5f * stepRotation;
        const auto endAngle = theta.Value() + stepRotation;
        const auto cosEnd = math::FastCosine(endAngle);
        const auto sinEnd = math::FastSine(endAngle);

        const auto vDq = park.Forward(clarke.Forward(phaseVoltages), math::FastCosine(midAngle), math::FastSine(midAngle));
        const auto iDqAtStart = park.Forward(clarke.Forward(foc::ThreePhase{ ia.Value(), ib.Value(), ic.Value() }), cosTheta, sinTheta);

        auto id = iDqAtStart.d;
        auto iq = iDqAtStart.q;

        const auto dIdDt = (vDq.d - winding.resistance * id + omega.Value() * winding.inductanceQ * iq) / winding.inductanceD;
        const auto dIqDt = (vDq.q - winding.resistance * iq - omega.Value() * winding.inductanceD * id - omega.Value() * winding.fluxLinkage) / winding.inductanceQ;

        id += dIdDt * dt;
        iq += dIqDt * dt;

        const auto iAbc = clarke.Inverse(park.Inverse(foc::RotatingFrame{ id, iq }, cosEnd, sinEnd));

        ia = foc::Ampere{ iAbc.a };
        ib = foc::Ampere{ iAbc.b };
        ic = foc::Ampere{ iAbc.c };

        ApplyOpenPhases();

        if (openPhases.Any())
        {
            const auto masked = park.Forward(clarke.Forward(foc::ThreePhase{ ia.Value(), ib.Value(), ic.Value() }), cosEnd, sinEnd);
            id = masked.d;
            iq = masked.q;
        }

        lastDqCurrents = foc::RotatingFrame{ id, iq };
        return iDqAtStart;
    }

    foc::PhaseCurrents ElectricalPlant::Currents() const
    {
        return { ia, ib, ic };
    }

    foc::PhaseCurrentSlopes ElectricalPlant::ZeroVectorSlopes(foc::Radians theta, foc::RadiansPerSecond omega, const WindingParameters& winding) const
    {
        const auto cosTheta = math::FastCosine(theta.Value());
        const auto sinTheta = math::FastSine(theta.Value());
        const auto current = park.Forward(clarke.Forward(foc::ThreePhase{ ia.Value(), ib.Value(), ic.Value() }), cosTheta, sinTheta);
        const auto speed = omega.Value();

        const foc::RotatingFrame rate{
            (-winding.resistance * current.d + speed * winding.inductanceQ * current.q) / winding.inductanceD - speed * current.q,
            (-winding.resistance * current.q - speed * winding.inductanceD * current.d - speed * winding.fluxLinkage) / winding.inductanceQ + speed * current.d
        };
        const auto phases = clarke.Inverse(park.Inverse(rate, cosTheta, sinTheta));

        return {
            foc::AmperePerSecond{ openPhases.a ? 0.0f : phases.a },
            foc::AmperePerSecond{ openPhases.b ? 0.0f : phases.b },
            foc::AmperePerSecond{ openPhases.c ? 0.0f : phases.c }
        };
    }

    foc::RotatingFrame ElectricalPlant::LastDqCurrents() const
    {
        return lastDqCurrents;
    }

    float ElectricalPlant::InverterPower(const foc::ThreePhase& phaseVoltages) const
    {
        return phaseVoltages.a * ia.Value() + phaseVoltages.b * ib.Value() + phaseVoltages.c * ic.Value();
    }

    float ElectricalPlant::CopperLoss(float resistance) const
    {
        return resistance * (ia.Value() * ia.Value() + ib.Value() * ib.Value() + ic.Value() * ic.Value());
    }

    void ElectricalPlant::Reset()
    {
        ia = foc::Ampere{ 0.0f };
        ib = foc::Ampere{ 0.0f };
        ic = foc::Ampere{ 0.0f };
    }

    void ElectricalPlant::ApplyOpenPhases()
    {
        if (openPhases.a)
            ia = foc::Ampere{ 0.0f };
        if (openPhases.b)
            ib = foc::Ampere{ 0.0f };
        if (openPhases.c)
            ic = foc::Ampere{ 0.0f };
    }
}
