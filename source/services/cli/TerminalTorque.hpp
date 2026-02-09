#pragma once

#include "services/util/TerminalWithStorage.hpp"
#include "source/foc/instantiations/FocImpl.hpp"
#include "source/services/cli/TerminalBase.hpp"

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
