#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"

namespace application
{
    class CalibrationContext
    {
    public:
        CalibrationContext(
            drivers::ThreePhaseInverter& inverter,
            foc::Volts vdc,
            foc::Weber configuredFluxLinkage);

        const services::CalibrationData& Data() const;
        services::CalibrationData& MutableData();
        void SetData(const services::CalibrationData& data);
        void Invalidate();

        bool IsComplete(bool modeSpecificValid) const;
        bool HasPartial() const;

        static bool HasFiniteElectricalParameters(const services::CalibrationData& data);

        bool IsRotorReferenceValid() const;
        void SetRotorReferenceValid(bool valid);

        float PendingFluxLinkage() const;
        void SetPendingFluxLinkage(float value);
        void CommitPendingFluxLinkage();

        bool Apply(foc::FocBase& foc, foc::CurrentLoopTunable& tunable);
        bool ApplyModel(
            foc::Ohm resistance,
            foc::MilliHenry inductance,
            std::size_t polePairs,
            float bandwidth,
            foc::Weber fluxLinkage,
            foc::FocBase& controller,
            foc::CurrentLoopTunable& tunable);

        foc::Weber EffectiveFluxLinkage() const;
        foc::Weber EffectiveFluxLinkage(const services::CalibrationData& data) const;
        foc::Weber ActiveFluxLinkage() const;

        float DefaultCurrentLoopBandwidth() const;
        foc::CurrentLoopTunings CurrentTuningsFor(float bandwidth) const;

        drivers::ThreePhaseInverter& GetInverter();
        foc::Volts GetVdc() const;

    private:
        drivers::ThreePhaseInverter& inverter;
        foc::Volts vdc;
        foc::Weber configuredFluxLinkage;

        services::CalibrationData calibrationData{};
        float pendingFluxLinkage{ 0.0f };
        bool rotorReferenceValid_{ false };

        static constexpr float nyquistFactor = 15.0f;
    };
}
