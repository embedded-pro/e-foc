#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "core/services/alignment/MotorAlignment.hpp"

namespace services
{
    class TerminalMotorAlignment
    {
    public:
        TerminalMotorAlignment(services::TerminalWithStorage& terminal, services::Tracer& tracer, MotorAlignment& alignment);

    private:
        using StatusWithMessage = services::TerminalWithStorage::StatusWithMessage;

        StatusWithMessage ForceAlignment(const infra::BoundedConstString& param);

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        MotorAlignment& alignment;
    };
}
