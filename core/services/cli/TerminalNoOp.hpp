#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalFocNoOpInteractor
    {
    public:
        TerminalFocNoOpInteractor(services::TerminalWithStorage&, foc::Volts, foc::FocBase&)
        {}
    };
}
