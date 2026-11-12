#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/flux_linkage_ident/FluxLinkageIdentificationImpl.hpp"
#include "core/state_machine/FocStateMachineDependencies.hpp"
#include <optional>

namespace application
{
    services::FluxLinkageIdentification* ResolveFluxLinkageIdent(
        const CalibrationServices& calibServices,
        std::optional<services::FluxLinkageIdentificationImpl>& ownFluxIdent,
        foc::CurrentCommandable& currentCommandable,
        foc::Controllable& drive,
        const foc::FocBase& motion,
        drivers::ThreePhaseInverter& inverter);
}
