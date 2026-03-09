#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "source/services/electrical_system_ident/ElectricalParametersIdentification.hpp"

namespace services
{
    class TerminalElectricalParametersIdentification
    {
    public:
        TerminalElectricalParametersIdentification(services::TerminalWithStorage& terminal, services::Tracer& tracer, ElectricalParametersIdentification& identification);

    private:
        using StatusWithMessage = services::TerminalWithStorage::StatusWithMessage;

        StatusWithMessage EstimateResistanceAndInductance(const infra::BoundedConstString& param);
        StatusWithMessage EstimateNumberOfPolePairs(const infra::BoundedConstString& param);

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        ElectricalParametersIdentification& identification;
    };
}
