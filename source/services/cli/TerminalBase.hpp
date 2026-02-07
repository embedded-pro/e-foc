#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "source/foc/interfaces/Controller.hpp"

namespace services
{
    class TerminalFocBaseInteractor
    {
    public:
        services::TerminalWithStorage& Terminal();

    protected:
        using StatusWithMessage = services::TerminalWithStorage::StatusWithMessage;

        TerminalFocBaseInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::ControllerBase& foc);

    private:
        StatusWithMessage SetFocPid(const infra::BoundedConstString& param);
        StatusWithMessage Start();
        StatusWithMessage Stop();

    private:
        services::TerminalWithStorage& terminal;
        foc::Volts vdc;
        foc::ControllerBase& foc;
    };
}
