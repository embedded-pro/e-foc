#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "core/services/cogging_compensation/CoggingMapping.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/flux_linkage_ident/FluxLinkageIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/standstill/RotorStandstill.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <cstdint>
#include <functional>
#include <optional>

namespace application
{
    struct TerminalAndTracer
    {
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
    };

    struct MotorHardware
    {
        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        foc::Volts vdc;
        uint32_t dutyUpdateDelayPeriods{ 0 };
    };

    // Kept across a mode switch out of an aligned Ready: the encoder zero is shared hardware.
    enum class RotorReference : uint8_t
    {
        lost,
        kept
    };

    struct CalibrationServices
    {
        services::ElectricalParametersIdentification& electricalIdent;
        services::MotorAlignment& motorAlignment;
        std::optional<std::reference_wrapper<services::MechanicalParametersIdentification>> mechIdentOverride{ std::nullopt };
        foc::Weber fluxLinkage{ foc::Weber{ 0.0f } };
        uint8_t agreeingMechanicalRuns{ 1 };
        std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets{ std::nullopt };
        bool mapCogging{ false };
        std::optional<std::reference_wrapper<services::CoggingMapping>> coggingMappingOverride{ std::nullopt };
        std::optional<std::reference_wrapper<services::RotorStandstill>> standstill{ std::nullopt };
        bool identifyFluxLinkage{ false };
        std::optional<std::reference_wrapper<services::FluxLinkageIdentification>> fluxIdentOverride{ std::nullopt };
    };
}
