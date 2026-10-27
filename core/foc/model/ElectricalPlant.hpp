#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/PowerStage.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"

namespace foc
{
    struct WindingParameters
    {
        float resistance;
        float inductanceD;
        float inductanceQ;
        float fluxLinkage;
    };

    struct OpenPhases
    {
        bool a{ false };
        bool b{ false };
        bool c{ false };

        bool Any() const
        {
            return a || b || c;
        }
    };

    class ElectricalPlant
    {
    public:
        void SetPowerStage(const PowerStageConfig& config);
        const PowerStageConfig& PowerStage() const;
        void SetOpenPhases(const OpenPhases& phases);

        foc::ThreePhase PhaseVoltages(const foc::PhasePwmDutyCycles& duties, foc::Volts bus, float period) const;
        foc::RotatingFrame Step(const foc::ThreePhase& phaseVoltages, foc::Radians theta, foc::RadiansPerSecond omega, const WindingParameters& winding, float dt);

        foc::PhaseCurrents Currents() const;
        foc::RotatingFrame LastDqCurrents() const;
        float InverterPower(const foc::ThreePhase& phaseVoltages) const;
        float CopperLoss(float resistance) const;
        void Reset();

    private:
        void ApplyOpenPhases();

    private:
        [[no_unique_address]] foc::Clarke clarke;
        [[no_unique_address]] foc::Park park;
        PowerStageConfig powerStage{};
        OpenPhases openPhases{};
        foc::Ampere ia{ 0.0f };
        foc::Ampere ib{ 0.0f };
        foc::Ampere ic{ 0.0f };
        foc::RotatingFrame lastDqCurrents{};
    };
}
