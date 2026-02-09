#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "source/services/cli/TerminalBase.hpp"

namespace services
{
    class TerminalFocSpeedInteractor
        : public TerminalFocBaseInteractor
    {
    public:
        TerminalFocSpeedInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::FocSpeed& foc);

    private:
        StatusWithMessage SetSpeedPid(const infra::BoundedConstString& param);
        StatusWithMessage SetSpeed(const infra::BoundedConstString& param);

    private:
        foc::Volts vdc;
        foc::FocSpeed& foc;
    };
}
