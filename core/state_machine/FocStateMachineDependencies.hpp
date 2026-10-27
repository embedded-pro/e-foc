#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "core/services/cogging_compensation/CoggingMapping.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/standstill/RotorStandstill.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"
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
    };

    // Kept by a control-mode switch out of an aligned Ready: the encoder zero the alignment set is
    // shared hardware and survives the machine that set it.
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
    };
}
