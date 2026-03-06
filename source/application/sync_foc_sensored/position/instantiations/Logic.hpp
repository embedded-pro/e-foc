#pragma once

#include "source/application/sync_foc_sensored/instantiations/LogicWithOuterLoop.hpp"
#include "source/foc/implementations/FocPositionImpl.hpp"
#include "source/services/cli/TerminalPosition.hpp"

namespace application
{
    class Logic
        : public LogicWithOuterLoop<foc::FocPositionImpl, services::TerminalFocPositionInteractor>
    {
    public:
        explicit Logic(application::HardwareFactory& hardware)
            : LogicWithOuterLoop(hardware, "sync_foc_sensored:position")
        {}
    };
}
