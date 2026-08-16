#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/services/cli/TerminalBase.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalFocTorqueInteractor
        : public TerminalFocBaseInteractor
    {
    public:
        TerminalFocTorqueInteractor(services::TerminalWithStorage& terminal, foc::FocTorque& torque);

    private:
        StatusWithMessage SetTorque(const infra::BoundedConstString& param);

    private:
        foc::FocTorque& foc;
    };
}
