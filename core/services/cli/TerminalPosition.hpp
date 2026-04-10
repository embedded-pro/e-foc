#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "core/services/cli/TerminalBase.hpp"

namespace services
{
    class TerminalFocPositionInteractor
        : public TerminalFocBaseInteractor
    {
    public:
        TerminalFocPositionInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::FocPosition& foc);

    private:
        StatusWithMessage SetSpeedPid(const infra::BoundedConstString& param);
        StatusWithMessage SetPositionPid(const infra::BoundedConstString& param);
        StatusWithMessage SetPosition(const infra::BoundedConstString& param);

    private:
        foc::Volts vdc;
        foc::FocPosition& foc;
    };
}
