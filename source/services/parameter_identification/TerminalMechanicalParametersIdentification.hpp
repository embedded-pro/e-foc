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

        StatusWithMessage EstimateFriction(const infra::BoundedConstString& param);
        StatusWithMessage EstimateInertia(const infra::BoundedConstString& param);

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        MechanicalParametersIdentification& identification;
        std::optional<foc::NewtonMeterSecondPerRadian> lastDamping;
    };
}
