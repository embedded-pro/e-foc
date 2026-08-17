#pragma once

#include "core/services/cli/TerminalBase.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalFocSpeedInteractor
        : public TerminalFocBaseInteractor
    {
    public:
        TerminalFocSpeedInteractor(services::TerminalWithStorage& terminal, foc::FocSpeed& foc);

    private:
        StatusWithMessage SetSpeedPid(const infra::BoundedConstString& param);
        StatusWithMessage SetSpeed(const infra::BoundedConstString& param);

    private:
        foc::FocSpeed& foc;
    };
}
