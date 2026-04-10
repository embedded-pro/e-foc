#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/services/cli/TerminalBase.hpp"

namespace services
{
    class TerminalFocTorqueInteractor
        : public TerminalFocBaseInteractor
    {
    public:
        TerminalFocTorqueInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::FocTorque& torque);

    private:
        StatusWithMessage SetTorque(const infra::BoundedConstString& param);

    private:
        foc::FocTorque& foc;
    };
}
