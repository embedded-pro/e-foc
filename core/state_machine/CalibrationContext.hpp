#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include <functional>
#include <optional>

namespace application
{
    class CalibrationContext
    {
    public:
        CalibrationContext(
            drivers::ThreePhaseInverter& inverter,
            foc::Volts vdc,
            foc::Weber configuredFluxLinkage,
            std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets = std::nullopt);

        const services::CalibrationData& Data() const;
        services::CalibrationData& MutableData();
        void SetData(const services::CalibrationData& data);
        void Invalidate();

        bool IsComplete(bool modeSpecificValid) const;
        bool HasPartial() const;

        static bool HasFiniteElectricalParameters(const services::CalibrationData& data);

        bool IsRotorReferenceValid() const;
        void SetRotorReferenceValid(bool valid);

        bool Apply(foc::FocBase& foc, foc::CurrentLoopTunable& tunable);
        void ApplyCurrentOffsets();
        bool ApplyModel(
            foc::Ohm resistance,
            foc::MilliHenry inductance,
            std::size_t polePairs,
            float bandwidth,
            foc::Weber fluxLinkage,
            foc::Ampere currentNoise,
            foc::FocBase& controller,
            foc::CurrentLoopTunable& tunable);

        foc::Weber EffectiveFluxLinkage() const;
        foc::Weber EffectiveFluxLinkage(const services::CalibrationData& data) const;
        foc::Weber ActiveFluxLinkage() const;

        float DefaultCurrentLoopBandwidth(foc::MilliHenry inductance, foc::Ampere currentNoise) const;
        foc::CurrentLoopTunings CurrentTuningsFor(float bandwidth, foc::MilliHenry inductance, foc::Ampere currentNoise) const;

    private:
        drivers::ThreePhaseInverter& inverter;
        foc::Volts vdc;
        foc::Weber configuredFluxLinkage;
        std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets;

        services::CalibrationData calibrationData{};
        bool rotorReferenceValid_{ false };

        static constexpr float nyquistFactor = 15.0f;
        static constexpr float inaudibleVoltageNoise = 0.08f;
        static constexpr float minimumAutomaticBandwidth = 10.0f * foc::SpeedLoopTunings{}.bandwidth;
    };
}
