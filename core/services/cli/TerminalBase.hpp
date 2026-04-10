#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalFocBaseInteractor
    {
    public:
        services::TerminalWithStorage& Terminal();

    protected:
        using StatusWithMessage = services::TerminalWithStorage::StatusWithMessage;

        TerminalFocBaseInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::FocBase& foc);

    private:
        StatusWithMessage SetFocPid(const infra::BoundedConstString& param);

    private:
        services::TerminalWithStorage& terminal;
        foc::Volts vdc;
        foc::FocBase& foc;
    };
}
