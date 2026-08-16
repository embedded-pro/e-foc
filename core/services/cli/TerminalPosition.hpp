#pragma once

#include "core/services/cli/TerminalBase.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalFocPositionInteractor
        : public TerminalFocBaseInteractor
    {
    public:
        TerminalFocPositionInteractor(services::TerminalWithStorage& terminal, foc::FocPosition& foc);

    private:
        StatusWithMessage SetSpeedPid(const infra::BoundedConstString& param);
        StatusWithMessage SetPositionPid(const infra::BoundedConstString& param);
        StatusWithMessage SetPosition(const infra::BoundedConstString& param);

    private:
        foc::FocPosition& foc;
    };
}
