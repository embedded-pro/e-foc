#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    // No-op TerminalImpl for FocStateMachineImpl when CLI is owned elsewhere.
    class TerminalFocNoOpInteractor
    {
    public:
        TerminalFocNoOpInteractor(services::TerminalWithStorage& /*terminal*/,
            foc::Volts /*vdc*/,
            foc::FocBase& /*foc*/)
        {}
    };
}
