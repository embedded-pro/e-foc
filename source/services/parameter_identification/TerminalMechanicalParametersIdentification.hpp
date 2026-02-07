#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "source/services/parameter_identification/MechanicalParametersIdentification.hpp"

namespace services
{
    class TerminalMechanicalParametersIdentification
    {
    public:
        TerminalMechanicalParametersIdentification(services::TerminalWithStorage& terminal, services::Tracer& tracer, MechanicalParametersIdentification& identification);

    private:
        using StatusWithMessage = services::TerminalWithStorage::StatusWithMessage;

        StatusWithMessage EstimateFrictionAndInertia(const infra::BoundedConstString& param);

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        MechanicalParametersIdentification& identification;
        std::optional<foc::NewtonMeterSecondPerRadian> lastDamping;
    };
}
