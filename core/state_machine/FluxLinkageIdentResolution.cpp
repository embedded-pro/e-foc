#include "core/state_machine/FluxLinkageIdentResolution.hpp"

namespace application
{
    services::FluxLinkageIdentification* ResolveFluxLinkageIdent(
        const CalibrationServices& calibServices,
        std::optional<services::FluxLinkageIdentificationImpl>& ownFluxIdent,
        foc::CurrentCommandable& currentCommandable,
        foc::Controllable& drive,
        const foc::FocBase& motion,
        drivers::ThreePhaseInverter& inverter)
    {
        if (calibServices.fluxIdentOverride.has_value())
            return &calibServices.fluxIdentOverride->get();

        if (!calibServices.identifyFluxLinkage)
            return nullptr;

        ownFluxIdent.emplace(currentCommandable, drive, motion, inverter);
        return &*ownFluxIdent;
    }
}
